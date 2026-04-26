#!lua name=dialog_lib

local function idx_key(id1, id2)
  if id1 < id2 then
    return 'dialog:idx:' .. id1 .. ':' .. id2
  else
    return 'dialog:idx:' .. id2 .. ':' .. id1
  end
end

-- FCALL dialog_send 0 <from_id> <to_id> <text> <msg_id>
redis.register_function('dialog_send', function(keys, args)
  local from_id = args[1]
  local to_id   = args[2]
  local text    = args[3]
  local msg_id  = args[4]
  local key = idx_key(from_id, to_id)
  redis.call('HSET', 'dialog:msg:' .. msg_id,
             'from', from_id, 'to', to_id, 'text', text)
  redis.call('RPUSH', key, msg_id)
  return redis.status_reply('OK')
end)

-- FCALL dialog_list 0 <user1_id> <user2_id>
-- Returns array of JSON strings: ["{\"from\":\"...\",\"to\":\"...\",\"text\":\"...\"}",  ...]
redis.register_function('dialog_list', function(keys, args)
  local user1 = args[1]
  local user2 = args[2]
  local key = idx_key(user1, user2)
  local ids = redis.call('LRANGE', key, 0, -1)
  local result = {}
  for _, msg_id in ipairs(ids) do
    local d = redis.call('HMGET', 'dialog:msg:' .. msg_id, 'from', 'to', 'text')
    table.insert(result, cjson.encode({from = d[1], to = d[2], text = d[3]}))
  end
  return result
end)
