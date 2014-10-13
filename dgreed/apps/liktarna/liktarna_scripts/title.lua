local title = {}

function title.update()
	if char.pressed(' ') or key.pressed(key._down)
		or key.pressed(key._up) or key.pressed(key._left)
		or key.pressed(key._right) then
		states.replace('game')
	end
	return true
end

function title.render(t)
	local alpha = 1 - math.abs(t)
	local white = rgba(1, 1, 1, alpha)
	sprsheet.draw('splash', 1, vec2(0, 0), white)
	return true
end

return title
