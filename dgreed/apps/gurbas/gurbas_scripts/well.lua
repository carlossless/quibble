well = {
	layer = 5,
	state = nil,
	shapes = nil,

	-- images
	img_block = nil
}

function well.init()
	well.img_block = tex.load(pre..'by.png')	
	well.reset()
end

function well.reset()
	well.state = {} 
	for x = 0,tiles_x-1 do
		well.state[widx(x, tiles_y-1)] = true	
	end
end

function well.draw()
	for y = 0,tiles_y-1 do
		for x = 0,tiles_x-1 do
			if well.state[widx(x, y)] ~= nil then
				local dest = vec2(x * tile_size, y * tile_size) 
				video.draw_rect(well.img_block, well.layer, dest)	
			end
		end
	end
end

-- returns true if block collides with well
function well.collide_block(b)
	for i,part in ipairs(b.shape) do
		if well.state[widx(part[0], part[1])] ~= nil then
			return true 
		end
	end
	return false 
end

function well.raycast(s, e)
	return e	
end

function well.close()
	tex.free(well.img_block)
end

