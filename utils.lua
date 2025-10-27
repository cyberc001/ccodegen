function random_int(_min, _max)
	if not _max then
		_max = _min
		_min = 0
	end
	return math.floor(math.random() * (_max - _min) + _min)
end


function node:get_children_filtered(filter, recursive)
	recursive = recursive or false

	local ch = {}
	for _, v in ipairs(self:get_children()) do
		if filter(v) then
			table.insert(ch, v)
		end

		if recursive then
			for _, v in ipairs(v:get_children_filtered(filter, recursive)) do
				table.insert(ch, v)
			end
		end
	end
	return ch
end

function node:get_children_of_type(_type, recursive)
	return self:get_children_filtered(function(v)
		return v._type == _type
	end, recursive)
end
function node:get_children_ops(op, recursive)
	return self:get_children_filtered(function(v)
		if v._type == nodes.cond then
			return op == tokens.cond
		end
		if v._type ~= nodes.un_op and v._type ~= nodes.bin_op then
			return false
		end
		return v.op == op
	end, recursive)
end
