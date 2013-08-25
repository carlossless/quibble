local spells = {}

local timeline = require('timeline')
local object = require('object')

local function move_cursor(dest_x, dest_y, last_keypress)
	if char.down('\r') then
		return dest_x, dest_y, last_keypress, true
	end

	if time.s() - last_keypress < 0.20 then
		return dest_x, dest_y, last_keypress, false
	end

	if char.pressed('h') or key.pressed(key._left) then
		return dest_x - 1, dest_y, time.s(), false
	end
	if (char.pressed('j') or key.pressed(key._down)) then
		return dest_x, dest_y + 1, time.s(), false
	end
	if char.pressed('k') or key.pressed(key._up) then
		return dest_x, dest_y - 1, time.s(), false
	end
	if char.pressed('l') or key.pressed(key._right) then
		return dest_x + 1, dest_y, time.s(), false
	end

	return dest_x, dest_y, last_keypress, false
end

local function select_target(current, n, last_keypress)
	if char.down('\r') then
		return current, last_keypress, true
	end

	if time.s() - last_keypress < 0.20 then
		return current, last_keypress, false
	end

	if char.pressed('j') or key.pressed(key._down) then
		local new = current+1
		if new > n then
			new = 1
		end
		return new, time.s(), false
	end
	if char.pressed('k') or key.pressed(key._up) then
		local new = current-1
		if new < 1 then
			new = n
		end
		return new, time.s(), false
	end

	return current, last_keypress, false
end

spells[1] = {
	name = 'Push',
	desc = 'Invisible force pushes objects or',
	desc2 = 'enemies away from you.',
	cost = 1,
	have = true,
	effect_len = 0.3,

	-- state
	push_objs = nil,
	pushes_to_make = 0,

	pre = function(player, room)
		local self = spells[1]
		local px = player.pos.x
		local py = player.pos.y
			-- get all movable objects within 3 squares
		local push_objs = {}
		for i,obj in ipairs(room.objs) do
			local d = obj_sqr_distance(obj, player)
			if obj.movable and d <= 3*3 then
				table.insert(push_objs, obj)
			end
		end

		-- sort from farthest to closest
		table.sort(push_objs, function(a, b)
			return obj_sqr_distance(a, player) > obj_sqr_distance(b, player)
		end)

		self.push_objs = push_objs
		self.pushes_to_make = 3
	end,
	effect = function(player, room, textmode, t)
		local f = 1-t
		local self = spells[1]
		room:render_circle(
			textmode, player.pos.x, player.pos.y, t * 4, rgba(0.5*f, 0.2*f, 0.7*f)
		)
		if (3 - self.pushes_to_make) < t * 3 then
			self.pushes_to_make = self.pushes_to_make - 1
			for i,obj in ipairs(self.push_objs) do
				local d = normalize(obj.pos - player.pos) 	
				d.x = round(d.x)
				d.y = round(d.y)
				local old_pos = obj.pos
				obj.pos = obj.pos + d

				if room:collide(obj.pos.x, obj.pos.y, true, true, true, obj) then
					obj.pos = old_pos
				end
			end
		end
		return spells[1]
	end,
	post = function(player, room)
		room:player_moved(player)
	end
}

spells[2] = {
	name = 'Fireball',
	desc = 'Conjure and throw a swirling',
	desc2 = 'orb of flame.',
	cost = 2,
	have = true,

	-- state
	effect_len = inf,
	dest_x = nil,
	dest_y = nil,
	last_keypress = 0,
	path = nil,

	pre = function(player, room)
		local self = spells[2] 
		self.effect_len = -1
		self.dest_x = player.pos.x
		self.dest_y = player.pos.y
		timeline.text2 = 'hjkl/arrows - target, enter - confirm' 
	end,
	effect = function(player, room, textmode, t)
		textmode:push()
		local self = spells[2] 
		if self.effect_len == -1 then
			-- target select
			local sel
			self.dest_x, self.dest_y, self.last_keypress, sel = move_cursor(
				self.dest_x, self.dest_y, self.last_keypress
			)

			textmode.selected_bg = rgba(0.4, 0.4, 0.4)
			textmode:recolour(self.dest_x, self.dest_y, 1)

			if sel then
				self.path, self.target = room:ray(
					player.pos.x, player.pos.y, self.dest_x, self.dest_y
				)
				self.effect_len = #self.path * 0.05
				room.spell_t = time.s()
				timeline.text2 = nil
			end
		else
			local tt = t * #self.path + 1
			for i,p in ipairs(self.path) do
				local c = (i - tt)
				
				if c >= -1 and c < 1 then
					textmode.selected_bg = rgba(0.9*math.sqrt(math.abs(1-c)), 0.1, 0.2)
					textmode:recolour(p[1], p[2], 1)
				end
			end
		end

		textmode:pop()

		return self
	end,
	post = function(player, room)
		local self = spells[2]
		if self.target then
			if self.target.die then
				self.target:die(room, player)
			end
			self.target = nil
		end
		room:player_moved(player)
	end
}

spells[3] = {
	name = 'Explode',
	desc = 'Makes a living creature explode,',
	desc2 = 'damaging everything nearby.',
	cost = 3,

	-- state
	effect_len = inf,
	target = 1,
	targets = nil,
	victims = nil,
	last_keypress = 0,

	pre = function(player, room)
		local self = spells[3]
		local targets = {}

		-- make list of potential targets
		for i,obj in ipairs(room.objs) do
			if obj.enemy and obj_sqr_distance(player, obj) < 8*8 then
				table.insert(targets, obj)
			end
		end

		-- sort by x then y
		table.sort(targets, function(a, b)
			if a.pos.x ~= b.pos.x then
				return a.pos.x < b.pos.x
			else
				return a.pos.y < b.pos.y
			end
		end)

		-- prep state for selecting target
		self.targets = targets
		self.effect_len = -1
		self.target = 1
		timeline.text2 = 'jk/arrows - target, enter - confirm'
	end,
	effect = function(player, room, textmode, t)
		textmode:push()
		local self = spells[3]
		if self.effect_len == -1 then
			-- target select
			local sel
			self.target, self.last_keypress, sel = select_target(
				self.target, #self.targets, self.last_keypress
			)

			local tg = self.targets[self.target]
			if not tg then
				textmode:pop()
				timeline.text = 'No nearby enemies to explode!'
				timeline.text_color = rgba(0, 0, 1)
				return nil
			end
			textmode.selected_bg = rgba(0.4, 0.4, 0.4)
			textmode:recolour(tg.pos.x, tg.pos.y, 1)

			if sel then
				self.effect_len = 0.4
				room.spell_t = time.s()
				timeline.text2 = nil

				-- prep list of victims
				local victims = {}
				for i,obj in ipairs(room.objs) do
					if obj.enemy and obj_sqr_distance(obj, tg) <= 2*2 then
						table.insert(victims, obj)
					end
				end
				
				-- sort by distance
				table.sort(victims, function(a, b)
					return obj_sqr_distance(a, tg) < obj_sqr_distance(b, tg)
				end)

				self.victims = victims
			end
		else
			local tg = self.targets[self.target]
			room:render_circle(
				textmode, tg.pos.x, tg.pos.y, t*2, rgba(1-t, 0, 0)
			)

			-- kill victims
			local tt = t * #self.victims
			for i,obj in ipairs(self.victims) do
				if not obj.remove and tt > i-1.1 then
					if obj.die then
						obj:die(room, player)
					end
				end
			end
		end
		textmode:pop()

		return self
	end,
	post = function(player, room)
		local self = spells[3]
		self.targets = nil
		self.victims = nil
		room:player_moved(player)
	end
}

spells[4] = {
	name = 'Summon',
	desc = 'Summon an army of skeletons to fight',
	desc2 = 'for you.',
	cost = 4,

	effect_len = 1,

	-- state
	to_spawn = 10,
	current = 0,
	next_position = nil,
	skip = false,

	pre = function(player, room)
		local self = spells[4]
		self.to_spawn = 10
		self.next_position = nil
		self.skip = false
		self.current = 0
	end,
	effect = function(player, room, textmode, t)
		local px, py = player.pos.x, player.pos.y
		local self = spells[4]
		local tt = t * 10

		if not self.next_position and not skip then
			local dx, dy
			local reps = 0
			repeat
				dx, dy = rand.int(-4, 5), rand.int(-4, 5)
				reps = reps + 1
				if reps > 100 then
					self.skip = true
					break
				end
			until dx*dx + dy*dy <= 4*4 and not room:collide(px+dx,py+dy, true, true, true)

			if not skip then
				self.next_position = vec2(px+dx, py+dy)
			end
		end

		local int, frac = math.modf(tt)
		if int ~= self.current then
			if self.next_position then
				table.insert(room.objs,
					object.make_obj(self.next_position, 'S')
				)
				self.next_position = nil
			end
			self.current = int
		end

		if self.next_position then
			textmode:push()
			textmode.selected_bg = rgba(0, 0, frac)
			textmode:recolour(self.next_position.x, self.next_position.y, 1)
			textmode:pop()
		end

		return self
	end,
	post = function(player, room)
		room:player_moved(player)
	end
}

spells[5] = {
	name = 'Teleport',
	desc = 'Vanish and appear somewhere else.',
	desc2 = '',
	cost = 5,

	effect_len = inf,
	dest_x = nil,
	dest_y = nil,
	last_keypress = 0,
	
	pre = function(player, room)
		local self = spells[5]
		self.effect_len = -1
		self.dest_x = player.pos.x
		self.dest_y = player.pos.y
		timeline.text2 = 'hjkl/arrows - target, enter - confirm'
	end,
	effect = function(player, room, textmode, t)
		textmode:push()
		local self = spells[5]

		local px, py = player.pos.x, player.pos.y

		if self.effect_len == -1 then
			-- target select
			local sel
			local new_x, new_y
			new_x, new_y, self.last_keypress, sel = move_cursor(
				self.dest_x, self.dest_y, self.last_keypress
			)

			if (px-new_x)^2 + (py-new_y)^2 <= 9*9 then
				self.dest_x = new_x
				self.dest_y = new_y
			end

			textmode.selected_bg = rgba(0.4, 0.4, 0.4)
			textmode:recolour(self.dest_x, self.dest_y, 1)

			if sel then
				if room:collide(self.dest_x, self.dest_y, true, true, true) then
					timeline.text = 'Can\'t teleport here!'
					timeline.text_color = rgba(0, 0, 1)
				else
					self.effect_len = 0.3
					room.spell_t = time.s()
					timeline.text2 = nil
				end
			end

		else
			-- effect
			textmode.selected_bg = rgba(t, t, t)
			textmode:recolour(self.dest_x, self.dest_y, 1)
			local nt = 1 - t
			textmode.selected_bg = rgba(nt, nt, nt)
			textmode:recolour(player.pos.x, player.pos.y, 1)
		end

		textmode:pop()
		return self
	end,
	post = function(player, room)
		local self = spells[5]
		player.pos.x = self.dest_x
		player.pos.y = self.dest_y
		room:player_moved(player)
	end
}

spells[6] = {
	name = 'Freeze',
	desc = 'Turn every living creature around you',
	desc2 = 'into a statue of ice.',
	cost = 6,

	-- state
	effect_len = 0.4,
	pre = nil,
	effect = function(player, room, textmode, t)
		local f = 1-t
		local self = spells[6]
		room:render_circle(
			textmode, player.pos.x, player.pos.y, t * 5, rgba(0.1*f, 0.1*f, 0.9*f)
		)

		return self
	end,
	post = function(player, room)
		for i,obj in ipairs(room.objs) do
			if obj.tick and obj_sqr_distance(obj, player) < 5.6^2 then
				obj.color = rgba(0, 0, 0.8)			
				obj.tick = nil
			end
		end
		room:player_moved(player)
	end
}

spells[7] = {
	name = 'Shatter',
	desc = 'Cast a powerful ray, destroying all',
	desc2 = 'in its path, including walls of stone.',
	cost = 7
}

spells[8] = {
	name = 'Spell8',
	desc = 'Description.',
	desc2 = '',
	cost = 8
}

spells[9] = {
	name = 'Ascend',
	desc = 'Become one with the stars, leaving your',
	desc2 = 'physical body behind.',
	cost = 9
}

spells.selected = 1

function spells.update(room)
	if char.down('j') or key.down(key._down) then
		spells.selected = math.min(9, spells.selected + 1) 
	end
	if char.down('k') or key.down(key._up) then
		spells.selected = math.max(1, spells.selected - 1)
	end

	for i=1,9 do
		if char.down(tostring(i)) then
			spells.selected = i
		end
	end

	if char.down('\r') then
		spells.delegate = function()
			spells.cast(spells.selected, room)
		end
		return false
	end

	return true
end

function spells.render(tm)
	tm:push()
	local color_selected = rgba(0.2, 0.2, 0.2)
	local color_bg = rgba(0.1, 0.1, 0.1)
	tm.selected_bg = color_bg
	tm.selected_fg = rgba(0.8, 0.3, 0.2)
	tm:write_middle(3, '======== Spells ========')
	for i=1,9 do
		tm:write_middle(3 + i, string.format(
			'| %d %s%s |', i, spells[i].name, string.rep(' ', 18 - #spells[i].name)
		))
		if spells.selected == i then
			tm.selected_bg = color_selected
			tm:recolour((40 - 24)/2, 3 + i, 24)
			tm.selected_bg = color_bg
		end
	end
	tm:write_middle(13, '========================')
	tm:pop()
	tm:write(0,18, spells[spells.selected].desc)
	tm:write(0,19, spells[spells.selected].desc2)
end

function spells.cast(i, room)
	assert(i >= 1 and i <= 9)
	--if spells[i].have then
		if timeline.current <= spells[i].cost then
			local txt
			if spells[i].cost == 1 then
				txt = string.format('Need one second to cast %s', spells[i].name)
			else
				txt = string.format('Need %s seconds to cast %s',
					spells[i].cost, spells[i].name
				)
			end

			timeline.text = txt
			timeline.text_color = rgba(0, 0, 1)
			return
		end

		room.spell = spells[i]
		room.spell_t = time.s()
		if spells[i].pre then
			local player = nil
			for i,o in ipairs(room.objs) do
				if o.char == '@' then
					player = o
				end
			end
			spells[i].pre(player, room)
		end
	--end
end

return spells
