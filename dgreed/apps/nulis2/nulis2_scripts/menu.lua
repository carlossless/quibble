module(..., package.seeall)

background_layer = 6
icons_layer = 7

color_white = rgba(1, 1, 1, 1)
color_selected = rgba(0.5, 0.5, 0.5, 0.8)

sprs = {}

-- touch
touch_up = nil
touch_current = nil
touch_sliding = false

-- positions
pos_replay = vec2(94, 174)
pos_sound = vec2(94, 314)
pos_music = vec2(94, 454)
pos_score = vec2(94, 594)
pos_levels = vec2(244, 174)
off_levels = vec2(140, 140)

-- ui state
state_sound = true
state_music = true

function init()
	sprs.background = sprsheet.get_handle('vignette')
	sprs.resume = sprsheet.get_handle('resume')
	sprs.replay = sprsheet.get_handle('replay')
	sprs.sound = sprsheet.get_handle('sound')
	sprs.sound_off = sprsheet.get_handle('sound_mute')
	sprs.music = sprsheet.get_handle('music')
	sprs.music_off = sprsheet.get_handle('music_mute')
	sprs.score = sprsheet.get_handle('score')
	sprs.back = sprsheet.get_handle('back')
	sprs.locked = sprsheet.get_handle('locked')

	local t, icon_rect = sprsheet.get(sprs.locked)
	half_icon_w, half_icon_h = (icon_rect.r - icon_rect.l)/2, (icon_rect.b - icon_rect.t)/2
end

function close()
end

function enter()
end

function leave()
end

function menu_icon(spr, alt_spr, pos, state, color, rot)
	local c = color
	local hit = false, false

	-- handle touch input
	if c == nil then
		c = color_white

		-- check for hover
		if touch_current and not touch_sliding then
			if math.abs(pos.x - touch_current.pos.x) < half_icon_w then
				if math.abs(pos.y - touch_current.pos.y) < half_icon_h then
					c = color_selected	
				end
			end
		end

		-- check for hit
		if touch_up and not touch_sliding then
			if math.abs(pos.x - touch_up.pos.x) < half_icon_w then
				if math.abs(pos.y - touch_up.pos.y) < half_icon_h then
					hit = true
				end
			end
		end
	end

	local sprite = spr
	if alt_spr ~= nil and state == false then
		sprite = alt_spr
	end

	sprsheet.draw_centered(sprite, icons_layer, pos, rot, 1.0, c) 

	if state == nil then
		return hit
	else
		-- emulate xor
		return not (state == hit)
	end
end

function draw_options()
	if menu_icon(sprs.replay, nil, pos_replay) then
		-- replay 
	end

	state_sound = menu_icon(sprs.sound, sprs.sound_off, pos_sound, state_sound) 
	state_music = menu_icon(sprs.music, sprs.music_off, pos_music, state_music) 

	if menu_icon(sprs.score, nil, pos_score) then
		-- score
	end
end

levels_off = 0
levels_off_target = 0

function draw_levels()
	if touch_up then
		local up_d = touch_up.pos.y - touch_up.hit_pos.y
		levels_off = levels_off + up_d
	end
	local off = levels_off 

	if not touch_current then
		levels_off = lerp(levels_off, levels_off_target, 0.1)
	end

	local fadeoff = off_levels.y / 2

	local d = 0
	if touch_current then
		d = touch_current.pos.y - touch_current.hit_pos.y
	end
	if touch_sliding and math.abs(d) > 8 then
		off = off + d
		levels_off_target = math.floor(0.5 + off / off_levels.y) * off_levels.y
		levels_off_target = clamp(-off_levels.y*4, 0, levels_off_target)
	end

	for y=1,8 do
		local y_pos = (y-1)*off_levels.y + off
		local alpha = 1

		-- fade out
		if y_pos < 0 then
			alpha = 1 + (y_pos / fadeoff)
		elseif y_pos > off_levels.y * 3 then
			alpha = 1 - ((y_pos - off_levels.y*3) / fadeoff)
		end

		if alpha > 0 then
			local col = nil
			if alpha ~= 1 then
				col = rgba(1, 1, 1, alpha)
			end
			for x=1,5 do
				local p = vec2(
					pos_levels.x + (x-1)*off_levels.x,
					pos_levels.y + y_pos
				)

				menu_icon(sprs.locked, nil, p, nil, col)
			end
		end
	end
end

function update()
	if key.up(key.quit) then
		states.pop()
	end

	-- update touch state
	if touch.count() == 1 then
		local t = touch.get(0)
		if touch_current == nil then
			touch_sliding = false
		end
		touch_current = t
		touch_up = nil

		if not touch_sliding and length_sq(t.pos - t.hit_pos) > 8*8 then
			touch_sliding = true
		end
	end

	if touch.count() == 0 then
		if touch_current ~= nil then
			touch_up = touch_current
		else
			touch_up = nil
		end
		touch_current = nil
	end

	return true
end

function render(t)
	local a = 1 - math.abs(t)
	local c = rgba(1, 1, 1, a)
	
	csim.render()

	sprsheet.draw(sprs.background, background_layer, rect(0, 0, scr_size.x, scr_size.y), c)

	draw_options()
	draw_levels()

	return true
end
