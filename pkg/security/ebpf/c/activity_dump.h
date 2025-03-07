#ifndef _ACTIVITY_DUMP_H_
#define _ACTIVITY_DUMP_H_

struct bpf_map_def SEC("maps/traced_cgroups") traced_cgroups = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = CONTAINER_ID_LEN,
    .value_size = sizeof(u64),
    .max_entries = 200, // might be overridden at runtime
};

struct traced_cgroups_counter_t {
    u64 max;
    u64 counter;
};

struct bpf_map_def SEC("maps/traced_cgroups_counter") traced_cgroups_counter = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(u32),
    .value_size = sizeof(struct traced_cgroups_counter_t),
    .max_entries = 1,
};

struct bpf_map_def SEC("maps/cgroup_wait_list") cgroup_wait_list = {
    .type = BPF_MAP_TYPE_LRU_HASH,
    .key_size = CONTAINER_ID_LEN,
    .value_size = sizeof(u64),
    .max_entries = 10, // might be overridden at runtime
};

struct bpf_map_def SEC("maps/traced_pids") traced_pids = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(u32),
    .value_size = sizeof(u64),
    .max_entries = 8192,
};

struct bpf_map_def SEC("maps/traced_comms") traced_comms = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = TASK_COMM_LEN,
    .value_size = sizeof(u64),
    .max_entries = 200,
};

struct bpf_map_def SEC("maps/traced_event_types") traced_event_types = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(u32),
    .value_size = sizeof(u64),
    .max_entries = EVENT_MAX + 1,
};

struct bpf_map_def SEC("maps/ad_dump_timeout") ad_dump_timeout = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(u32),
    .value_size = sizeof(u64),
    .max_entries = 1,
};

#define DURATION_30_MINUTES (30 * 60 * 1000000000ull)

__attribute__((always_inline)) u64 get_dump_timeout() {
    u32 key = 0;
    u64 *value = bpf_map_lookup_elem(&ad_dump_timeout, &key);
    if (!value || *value == 0) {
        return DURATION_30_MINUTES;
    }
    return *value;
}

__attribute__((always_inline)) u64 is_cgroup_activity_dumps_enabled() {
    u64 cgroup_activity_dumps_enabled;
    LOAD_CONSTANT("cgroup_activity_dumps_enabled", cgroup_activity_dumps_enabled);
    return cgroup_activity_dumps_enabled != 0;
}

__attribute__((always_inline)) u64 lookup_or_delete_traced_pid_timeout(u32 pid, u64 now) {
    u64 *timeout = bpf_map_lookup_elem(&traced_pids, &pid);
    if (timeout == NULL) {
        return 0;
    }

    if (now > *timeout) {
        // delete entry
        bpf_map_delete_elem(&traced_pids, &pid);
        return 0;
    }
    return *timeout;
}

__attribute__((always_inline)) u64 update_traced_pid_timeout(u32 pid, u64 new_timeout) {
    u64 *old_timeout = bpf_map_lookup_elem(&traced_pids, &pid);
    if (old_timeout != NULL) {
        if (new_timeout <= *old_timeout) {
            // nothing to do
            return *old_timeout;
        }
        new_timeout = *old_timeout;
    }
    bpf_map_update_elem(&traced_pids, &pid, &new_timeout, BPF_ANY);
    return new_timeout;
}

struct cgroup_tracing_event_t {
    struct kevent_t event;
    struct container_context_t container;
    u64 timeout;
};

struct bpf_map_def SEC("maps/cgroup_tracing_event_gen") cgroup_tracing_event_gen = {
    .type = BPF_MAP_TYPE_PERCPU_ARRAY,
    .key_size = sizeof(u32),
    .value_size = sizeof(struct cgroup_tracing_event_t),
    .max_entries = 1,
    .pinning = 0,
    .namespace = "",
};

__attribute__((always_inline)) struct cgroup_tracing_event_t *get_cgroup_tracing_event() {
    u32 key = 0;
    struct cgroup_tracing_event_t *evt = bpf_map_lookup_elem(&cgroup_tracing_event_gen, &key);
    if (evt == NULL) {
        return 0;
    }
    evt->container.container_id[0] = 0;
    return evt;
}

struct bpf_map_def SEC("maps/traced_cgroups_lock") traced_cgroups_lock = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(u32),
    .value_size = sizeof(u32),
    .max_entries = 1,
};

__attribute__((always_inline)) bool lock_cgroups_counter() {
    u32 key = 0;
    return bpf_map_update_elem(&traced_cgroups_lock, &key, &key, BPF_NOEXIST) == 0;
}

__attribute__((always_inline)) void unlock_cgroups_counter() {
    u32 key = 0;
    bpf_map_delete_elem(&traced_cgroups_lock, &key);
}

__attribute__((always_inline)) bool reserve_traced_cgroup_spot(char cgroup[CONTAINER_ID_LEN], u64 timeout) {
    if (!lock_cgroups_counter()) {
        return false;
    }

    void *already_in = bpf_map_lookup_elem(&traced_cgroups, &cgroup[0]);
    if (already_in) {
        goto fail;
    }

    u32 key = 0;
    struct traced_cgroups_counter_t *counter = bpf_map_lookup_elem(&traced_cgroups_counter, &key);
    if (!counter) {
        goto fail;
    }

    bool res = false;
    if (counter->counter < counter->max) {
        counter->counter++;
        res = true;
    } else {
        goto fail;
    }

    int ret = bpf_map_update_elem(&traced_cgroups, &cgroup[0], &timeout, BPF_NOEXIST);
    if (ret < 0) {
        // this should be caught earlier but we're already tracing too many cgroups concurrently, ignore this one for now
        goto fail;
    }

    unlock_cgroups_counter();
    return res;

fail:
    unlock_cgroups_counter();
    return false;
}

__attribute__((always_inline)) void freeup_traced_cgroup_spot(char cgroup[CONTAINER_ID_LEN]) {
    if (!lock_cgroups_counter()) {
        return;
    }

    bpf_map_delete_elem(&traced_cgroups, &cgroup[0]);

    u32 key = 0;
    struct traced_cgroups_counter_t *counter = bpf_map_lookup_elem(&traced_cgroups_counter, &key);
    if (counter && counter->counter > 0) {
        counter->counter -= 1;
    }

    unlock_cgroups_counter();
}

__attribute__((always_inline)) u64 trace_new_cgroup(void *ctx, char cgroup[CONTAINER_ID_LEN]) {
    u64 timeout = bpf_ktime_get_ns() + get_dump_timeout();

    if (!reserve_traced_cgroup_spot(cgroup, timeout)) {
        // we're already tracing too many cgroups concurrently, ignore this one for now
        return 0;
    }

    // send cgroup tracing event
    struct cgroup_tracing_event_t *evt = get_cgroup_tracing_event();
    if (evt == NULL) {
        // should never happen, ignore
        return 0;
    }
    copy_container_id(cgroup, evt->container.container_id);
    evt->timeout = timeout;
    send_event_ptr(ctx, EVENT_CGROUP_TRACING, evt);

    // return the new timeout
    return timeout;
}

__attribute__((always_inline)) void should_trace_new_process_comm(void *ctx, u64 now, u32 pid, char comm[TASK_COMM_LEN]) {
    // should we start tracing this comm ?
    u64 *dump_timeout = bpf_map_lookup_elem(&traced_comms, &comm[0]);
    if (dump_timeout) {
        if (now > *dump_timeout) {
            // remove expired comm entry
            bpf_map_delete_elem(&traced_comms, &comm[0]);
        } else {
            // we're still tracing this comm, update the pid timeout
            update_traced_pid_timeout(pid, *dump_timeout);
        }
    }
}

__attribute__((always_inline)) void should_trace_new_process_cgroup(void *ctx, u64 now, u32 pid, char cgroup[CONTAINER_ID_LEN]) {
    // should we start tracing this cgroup ?
    if (is_cgroup_activity_dumps_enabled() && cgroup[0] != 0) {
        // is this cgroup traced ?
        u64 *dump_timeout = bpf_map_lookup_elem(&traced_cgroups, &cgroup[0]);
        if (dump_timeout) {
            if (now > *dump_timeout) {
                // delete expired cgroup entry
                freeup_traced_cgroup_spot(cgroup);
            } else {
                // We're still tracing this cgroup, update the pid timeout
                update_traced_pid_timeout(pid, *dump_timeout);
            }
        } else {
            // have we seen this cgroup before ?
            u64 *wait_timeout = bpf_map_lookup_elem(&cgroup_wait_list, &cgroup[0]);
            if (wait_timeout) {
                if (now > *wait_timeout) {
                    // delete expired wait_list entry
                    bpf_map_delete_elem(&cgroup_wait_list, &cgroup[0]);
                } else {
                    // this cgroup is on the wait list, do not start tracing it
                    return;
                }
            }

            // can we start tracing this cgroup ?
            u64 timeout = trace_new_cgroup(ctx, cgroup);
            if (timeout > 0) {
                // a lock was acquired for this cgroup, start tracing the current pid
                update_traced_pid_timeout(pid, timeout);
            }
        }
    }
}

__attribute__((always_inline)) void should_trace_new_process(void *ctx, u64 now, u32 pid, char cgroup[CONTAINER_ID_LEN], char comm[TASK_COMM_LEN]) {
    should_trace_new_process_comm(ctx, now, pid, comm);
    should_trace_new_process_cgroup(ctx, now, pid, cgroup);
}


__attribute__((always_inline)) void inherit_traced_state(void *ctx, u32 ppid, u32 pid, char cgroup[CONTAINER_ID_LEN], char comm[TASK_COMM_LEN]) {
    u64 now = bpf_ktime_get_ns();
    should_trace_new_process(ctx, now, pid, cgroup, comm);

    // check if the parent is traced, update the child timeout if need be
    u64 parent_dump_timeout = lookup_or_delete_traced_pid_timeout(ppid, now);
    if (parent_dump_timeout > 0) {
        update_traced_pid_timeout(pid, parent_dump_timeout);
    }
}

__attribute__((always_inline)) void cleanup_traced_state(u32 pid) {
    // delete pid from traced_pids
    bpf_map_delete_elem(&traced_pids, &pid);
}

#define NO_ACTIVITY_DUMP       0
#define IGNORE_DISCARDER_CHECK 1

union container_id_comm_combo {
    char container_id[CONTAINER_ID_LEN];
    char comm[TASK_COMM_LEN];
};

__attribute__((always_inline)) void fill_activity_dump_discarder_state(void *ctx, struct is_discarded_by_inode_t *params) {
    struct proc_cache_t *pc = get_proc_cache(params->tgid);
    if (pc) {
        union container_id_comm_combo buffer = {};

        // prepare comm and cgroup (for compatibility with old kernels)
        bpf_probe_read(&buffer.comm, sizeof(buffer.comm), pc->entry.comm);
        should_trace_new_process_comm(ctx, params->now, params->tgid, buffer.comm);

        bpf_probe_read(&buffer.container_id, sizeof(buffer.container_id), pc->container.container_id);
        should_trace_new_process_cgroup(ctx, params->now, params->tgid, buffer.container_id);
    }

    u64 timeout = lookup_or_delete_traced_pid_timeout(params->tgid, params->now);
    if (timeout == 0) {
        params->activity_dump_state = NO_ACTIVITY_DUMP;
        return;
    }

    // is this event type traced ?
    u64 event_type = params->event_type;
    u64 *traced = bpf_map_lookup_elem(&traced_event_types, &event_type);
    if (traced == NULL) {
        params->activity_dump_state = NO_ACTIVITY_DUMP;
        return;
    }

    // set IGNORE_DISCARDER_CHECK
    params->activity_dump_state = IGNORE_DISCARDER_CHECK;
}

#endif
