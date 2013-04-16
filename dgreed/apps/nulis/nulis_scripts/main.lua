require 'game'

pre = 'nulis_assets/'
scr_size = {
	x = 1024,
	y = 768
}	

function game_init()
	-- reduce screen size if we don't have enough space
	local w, h = video.native_resolution()
	local reduction = 1
	local fullscreen = false

	if w == scr_size.x and h == scr_size.y then
		fullscreen = true
	end

	if not fullscreen and h <= scr_size.y then
		reduction = 2
	end

	-- init video, sound
	sound.init()
	video.init_ex(
		scr_size.x / reduction, scr_size.y / reduction, 
		scr_size.x, scr_size.y, 'nulis', fullscreen 
	)

	music = sound.load_stream(pre..'theme.ogg')
	sound.set_volume(music, 0.3)
	sound.play(music, true)

	-- register and enter game state
	states.register('game', game)
	states.push('game')
end

function game_close()
	sound.free(music)
	video.close()
	sound.close()
end
