#!lua name=counter_lib

-- FCALL counter_reset_dialog 0 <user_id> <partner_id>
-- Atomically: reads dialog:{partner_id} count, subtracts from total, zeroes the dialog field.
-- Returns the dialog count that was cleared.
redis.register_function('counter_reset_dialog', function(keys, args)
  local user_id    = args[1]
  local partner_id = args[2]
  local hash_key   = 'counters:' .. user_id
  local dialog_field = 'dialog:' .. partner_id

  local dialog_count = tonumber(redis.call('HGET', hash_key, dialog_field)) or 0
  if dialog_count > 0 then
    redis.call('HSET', hash_key, dialog_field, 0)
    local new_total = redis.call('HINCRBY', hash_key, 'total', -dialog_count)
    if tonumber(new_total) < 0 then
      redis.call('HSET', hash_key, 'total', 0)
    end
  end
  return dialog_count
end)

-- FCALL counter_reconcile_user 0 <user_id>
-- Recomputes total as the sum of all dialog:* fields in the hash.
-- Used by the periodic reconciler to correct drift from failed operations.
redis.register_function('counter_reconcile_user', function(keys, args)
  local user_id  = args[1]
  local hash_key = 'counters:' .. user_id
  local all = redis.call('HGETALL', hash_key)
  local computed_total = 0
  local i = 1
  while i <= #all do
    local field = all[i]
    local value = tonumber(all[i + 1]) or 0
    if field ~= 'total' and value > 0 then
      computed_total = computed_total + value
    end
    i = i + 2
  end
  redis.call('HSET', hash_key, 'total', computed_total)
  return computed_total
end)
