dofile(src..'well.lua')
dofile(src..'block.lua')
dofile(src..'guy.lua')

game = {}

-- gets called once at the game start;
-- right place to load resources
function game.init()
	well.init()
	block.init()
	guy.init()
end

-- called on game exit,
-- all resources should be freed here
function game.close()
	block.close()
	well.close()
	guy.close()
end

-- called repeatedly from game loop
function game.frame()
	well.draw()

	block.update()
	block.draw()

	guy.update()
	guy.draw()
end

