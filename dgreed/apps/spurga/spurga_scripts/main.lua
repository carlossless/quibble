function params_find(param)
	if argv then
		for i,p in ipairs(argv) do
			if p == param then
				return true
			end
		end
	end
	return false
end

function setup_screen()
	local w, h = video.native_resolution()
	if params_find('-retina') or (w == 960 and h == 640) then
		retina = true
	end	
end

pre = 'spurga_assets/'
scr_size = vec2(320, 480) 
grid_pos = vec2(scr_size.x / 2, 32 + 384/2)

retina = nil
ipad = nil

setup_screen()

local hud = require('hud')
local game = require('game')
local menu = require('menu')
local levels = require('levels')
local scores = require('scores')
local puzzles = require('puzzles')

-- transition mask is list of numbers from 1 to n, shuffled randomly
transition_mask = nil

function new_transition_mask(n)
	-- generates new transition mask using Knuth's Algorithm P
	transition_mask = {}
	for i=1,n do
		transition_mask[i] = i
	end

	for i=n,1,-1 do
		local j = rand.int(1, i+1)
		transition_mask[i], transition_mask[j] = transition_mask[j], transition_mask[i]	
	end
end

local string_byte = string.byte
local string_byte_a = string_byte('a')
local string_byte_z = string_byte('z')
local string_byte_A = string_byte('A')
local string_byte_Z = string_byte('Z')

function is_portal(c)
	local n = string_byte(c)
	return n >= string_byte_a and n <= string_byte_z
end

function is_wall(c)
	local n = string_byte(c)
	return n >= string_byte_A and n <= string_byte_Z
end

function game_init()
	states.register('game', game)
	states.register('menu', menu)
	states.register('levels', levels)
	states.register('scores', scores)

	hud.init()
	puzzles.load(pre..'puzzles.mml', pre..'slices.mml')

	if retina then
		fnt = font.load(pre..'HelveticaNeueLTCom-Md_29px.bft', 0.5, pre)
	else
		fnt = font.load(pre..'HelveticaNeueLTCom-Md_14px.bft', 1.0, pre)
	end

	new_transition_mask(6*5)
	
	states.transition_len = 0.4 
	states.push('menu')
end

function game_close()
	font.free(fnt)
	puzzles.free()
	hud.close()
end