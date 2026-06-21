# left tasks in prev queue

Edge case worth thinking about now even if you don't solve it yet: what happens to tasks still sitting in DSQ_FCFS after you flip active_policy_g to POLICY_RR? With this simple dispatch, they just sit there forever, because dispatch only ever consumes the currently active DSQ. That's the "stuck leftovers" problem from earlier. For your first working version, an easy partial fix: in dispatch, try the active DSQ first, and if nothing was consumed, fall through and try the others too. That's strategy (a) from before — simple, no data loss, slightly impure timing.

Bug 3: dropping the return value of scx_bpf_dsq_move_to_local
It returns a bool — whether it actually moved a task. Right now you ignore it. This matters once you implement the "leftovers from the previous policy" fallback we discussed earlier: if the move from the active policy's DSQ fails (empty), you'd want to try the other DSQ(s) before giving up, so tasks queued under a policy that's no longer active still eventually run. Not fatal for compiling/loading, but it's the next thing you'll want once policy switching is actually live, otherwise switching policy mid-run silently strands whatever was still queued in the old DSQ.

## metrics in user space

## need to make sure, prev quees are drained before any switch is made
