/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rDSN) -=- 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "simple_stateful_load_balancer.h"
#include <algorithm>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "load.balancer"

simple_stateful_load_balancer::simple_stateful_load_balancer(server_state* state):
    ::dsn::dist::server_load_balancer(state),
    serverlet<simple_stateful_load_balancer>("simple_stateful_load_balancer")
{
}

simple_stateful_load_balancer::~simple_stateful_load_balancer()
{
}

void simple_stateful_load_balancer::run()
{
    if (s_disable_lb) return;

    zauto_write_lock l(_state->_lock);

    for (size_t i = 0; i < _state->_apps.size(); i++)
    {
        app_state& app = _state->_apps[i];
        if (app.status != AS_AVAILABLE)
            continue;
        for (int j = 0; j < app.partition_count; j++)
        {
            partition_configuration& pc = app.partitions[j];
            run_lb(pc);
        }
    }
}

void simple_stateful_load_balancer::run(global_partition_id gpid)
{
    if (s_disable_lb) return;

    zauto_write_lock l(_state->_lock);
    partition_configuration& pc = _state->_apps[gpid.app_id - 1].partitions[gpid.pidx];
    run_lb(pc);
}

void simple_stateful_load_balancer::run_lb(partition_configuration& pc)
{
    if (_state->freezed())
        return;

    configuration_update_request proposal;
    partition_assist_info& assist_info = _state->_apps[pc.gpid.app_id - 1].partition_assists[pc.gpid.pidx];
    proposal.config = pc;

    if (pc.primary.is_invalid())
    {
        if (pc.secondaries.size() > 0)
        {
            if (s_lb_for_test)
            {
                std::vector< ::dsn::rpc_address> tmp(pc.secondaries);
                std::sort(tmp.begin(), tmp.end());
                proposal.node = tmp[0];
            }
            else
            {
                proposal.node = pc.secondaries[dsn_random32(0, static_cast<int>(pc.secondaries.size()) - 1)];
            }
            proposal.type = CT_UPGRADE_TO_PRIMARY;
        }

        else if (pc.last_drops.size() == 0)
        {
            proposal.node = find_minimal_load_machine(pc, true);
            proposal.type = CT_ASSIGN_PRIMARY;
        }

        // DDD
        else
        {
            proposal.node = *pc.last_drops.rbegin();
            proposal.type = CT_ASSIGN_PRIMARY;

            derror("%s.%d.%d enters DDD state, we are waiting for its last primary node %s to come back ...",
                pc.app_type.c_str(),
                pc.gpid.app_id,
                pc.gpid.pidx,
                proposal.node.to_string()
                );
        }

        if (proposal.node.is_invalid() == false)
        {
            send_proposal(proposal.node, proposal);
        }
    }

    else if (static_cast<int>(pc.secondaries.size()) + 1 < pc.max_replica_count)
    {
        proposal.node.set_invalid();
        if (static_cast<int>(pc.secondaries.size()) + 1 >= mutation_2pc_min_replica_count)
        {
            while (!assist_info.history_queue.empty())
            {
                dropout_history d = assist_info.history_queue.front();
                if (d.dropout_time + replica_assign_delay_ms_for_dropouts > dsn_now_ms())
                    return;
                assist_info.history_queue.pop_front();
                if (_state->is_node_alive(d.addr))
                {
                    proposal.node = d.addr;
                    break;
                }
            }
        }
        else
        {
            while (proposal.node.is_invalid() && !assist_info.history_queue.empty())
            {
                dropout_history& d = assist_info.history_queue.front();
                if (_state->is_node_alive(d.addr))
                    proposal.node = d.addr;
                assist_info.history_queue.pop_front();
            }
        }

        proposal.type = CT_ADD_SECONDARY;

        if (proposal.node.is_invalid())
            proposal.node = find_minimal_load_machine(pc, false);

        if (proposal.node.is_invalid() == false &&
            proposal.node != pc.primary &&
            std::find(pc.secondaries.begin(), pc.secondaries.end(), proposal.node) == pc.secondaries.end())
        {
            send_proposal(pc.primary, proposal);
        }
    }
    else
    {
        // it is healthy, nothing to do
    }
}

void simple_stateful_load_balancer::query_decree(std::shared_ptr<query_replica_decree_request> query)
{
    rpc::call(
        query->node,
        RPC_QUERY_PN_DECREE,
        *query,
        this,
        [this, query](error_code err, query_replica_decree_response&& resp) 
        {
            auto response = std::make_shared<query_replica_decree_response>(std::move(resp));
            on_query_decree_ack(err, query, response);
        }
        ,
        gpid_to_hash(query->gpid), std::chrono::seconds(3));
}

void simple_stateful_load_balancer::on_query_decree_ack(error_code err, const std::shared_ptr<query_replica_decree_request>& query, const std::shared_ptr<query_replica_decree_response>& resp)
{
    if (err != ERR_OK)
    {
        tasking::enqueue(LPC_QUERY_PN_DECREE, this, std::bind(&simple_stateful_load_balancer::query_decree, this, query), 0, std::chrono::seconds(1));
    }
    else
    {
        zauto_write_lock l(_state->_lock);
        app_state& app = _state->_apps[query->gpid.app_id - 1];
        partition_configuration& ps = app.partitions[query->gpid.pidx];
        if (resp->last_decree > ps.last_committed_decree)
        {
            ps.last_committed_decree = resp->last_decree;
        }   
    }
}
