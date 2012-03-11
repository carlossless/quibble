module(..., package.seeall)

require 'menu'

fadein_len = 3

background_layer = 6
text_layer = 7

color_white = rgba(1, 1, 1, 1)
color_gray = rgba(0.8, 0.8, 0.8, 1)
text_color = color_white

sprs = {}

function is_unlocked()
	return keyval.get('unlocked', false)
end

function products_cb(products)
	local p = products[1]

	if p then
		if scr_type == 'ipad' then
			buy_text = 'buy the rest of the game for '..p.price_str
		else
			buy_text = 'buy the game for '..p.price_str
		end
	end
end

function purchase_cb(id, success)
	log.info('Purchase!')
	if id == 'com.qbcode.nulis.unlock' and success then
		log.info('Unlocking')
		keyval.set('unlocked', true)
		states.pop()
	end

	if not success then
		purchased = false
		buy_text = old_buy_text
	end
end

function init()
	if iap then
		iap.init(products_cb, purchase_cb)
	end

	sprs.background = sprsheet.get_handle('background')
	sprs.text = sprsheet.get_handle('hint4')

	sprs.line_tex, sprs.line = sprsheet.get('stripe')

	buy_enter_t = -100
	held_down = false
end

function close()
	if iap then
		iap.close()
	end
end

function enter()
	buy_enter_t = time.s()
	held_down = false

	if old_buy_text then
		buy_text = old_buy_text
	end
end

function leave()
end

function update()
	if scr_type == 'ipad' and touch.count() == 5 then
		states.pop()
	end

	if scr_type == 'iphone' and touch.count() == 3 then
		states.pop()
	end

	-- hack for PCs 
	if char.down('u') then
		keyval.set('unlocked', true)
		states.pop()
	end

	-- buy logic
	if touch.count() == 1 then
		local t = touch.get(0)
		held_down = true
		text_color = color_gray
	end

	if touch.count() == 0 and held_down and buy_text ~= 'please wait...' then
		held_down = false
		text_color = color_white
		if iap and iap.is_active() then
			iap.purchase('com.qbcode.nulis.unlock')
			old_buy_text = buy_text
			buy_text = 'please wait ...'
		else
			os.alert('Unable to purchase', 'You must be connected to internet to purchase the rest of the game')
			states.pop()
		end
	end

	menu.update_orientation()

	return true
end

local center = vec2(scr_size.x / 2, scr_size.y / 2)

buy_text = 'buy the game for $0.99'
if scr_type == 'ipad' then
	buy_text = 'buy the rest of the game for $0.99'
end

local text_hack = 0
if retina then
	text_hack = 3
end

local line1_off = 12 
local line2_off = 42
if scr_type == 'ipad' then
	line1_off = 27
	line2_off = 77
	text_hack = 2
	if retina then
		text_hack = 6
	end
end
local off = (line1_off + line2_off) / 2 

local color_white = rgba(1, 1, 1, 1)

function render(t)
	local tt = clamp(0, 1, (time.s() - buy_enter_t) / fadein_len)
	local col = rgba(1, 1, 1, tt)

	sprsheet.draw(sprs.background, background_layer, rect(0, 0, scr_size.x, scr_size.y))

	sprsheet.draw_centered(sprs.text, text_layer, 
		center + rotate(vec2(0, -off), menu.angle),
		menu.angle, 1.0, col
	)

	local w, h = font.size(fnt, buy_text)

	text_color.a = col.a

	video.draw_text_rotated(fnt, text_layer, buy_text, 
		center + rotate(vec2(0, off + h + text_hack), menu.angle),
		menu.angle, 1.0, text_color
	)

	local line_p = center + rotate(vec2(0, line1_off), menu.angle)

	video.draw_rect(sprs.line_tex, text_layer, sprs.line,
		rect(line_p.x - w/2, line_p.y-1, line_p.x + w/2, line_p.y+1),
		menu.angle, text_color
	)

	local line_p = center + rotate(vec2(0, line2_off), menu.angle)

	video.draw_rect(sprs.line_tex, text_layer, sprs.line,
		rect(line_p.x - w/2, line_p.y-1, line_p.x + w/2, line_p.y+1),
		menu.angle, text_color
	)

	text_color.a = 1

	return true	
end