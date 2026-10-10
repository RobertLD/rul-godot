extends SceneTree

enum Tile { WATER, SAND, GRASS }

func _init() -> void:
	var fails := 0
	if not ClassDB.class_exists("WaveFunctionCollapse"):
		print("MISSING CLASS"); fails += 1

	var wfc := WaveFunctionCollapse.new()
	wfc.allow_all(Tile.WATER, Tile.WATER)
	wfc.allow_all(Tile.WATER, Tile.SAND)
	wfc.allow_all(Tile.SAND, Tile.SAND)
	wfc.allow_all(Tile.SAND, Tile.GRASS)
	wfc.allow_all(Tile.GRASS, Tile.GRASS)
	wfc.set_weight(Tile.WATER, 1.5)

	if wfc.get_weight(Tile.WATER) != 1.5: print("bad get_weight"); fails += 1
	if wfc.get_weight(Tile.GRASS) != 1.0: print("bad default weight"); fails += 1
	if not wfc.is_allowed(Tile.WATER, WaveFunctionCollapse.NORTH, Tile.SAND):
		print("allow_all missed NORTH"); fails += 1
	if not wfc.is_allowed(Tile.SAND, WaveFunctionCollapse.SOUTH, Tile.WATER):
		print("allow is not symmetric"); fails += 1
	if wfc.is_allowed(Tile.WATER, WaveFunctionCollapse.EAST, Tile.GRASS):
		print("water/grass should be illegal"); fails += 1

	var size := Vector2i(24, 16)
	var grid := wfc.generate(size, 1234)
	if grid.size() != size.x * size.y: print("bad grid size ", grid.size()); fails += 1

	# determinism
	if wfc.generate(size, 1234) != grid: print("same seed differed"); fails += 1
	if wfc.generate(size, 99) == grid: print("different seed matched"); fails += 1

	# every neighbouring pair must be legal
	for y in size.y:
		for x in size.x:
			var a: int = grid[y * size.x + x]
			if a < 0 or a > 2: print("bad state ", a); fails += 1
			if x + 1 < size.x:
				var b: int = grid[y * size.x + x + 1]
				if not wfc.is_allowed(a, WaveFunctionCollapse.EAST, b):
					print("illegal E pair at ", Vector2i(x, y)); fails += 1
			if y + 1 < size.y:
				var c: int = grid[(y + 1) * size.x + x]
				if not wfc.is_allowed(a, WaveFunctionCollapse.SOUTH, c):
					print("illegal S pair at ", Vector2i(x, y)); fails += 1

	# directional rule: 1 may only sit east of 0
	var dir := WaveFunctionCollapse.new()
	dir.allow(0, WaveFunctionCollapse.EAST, 1)
	if dir.is_allowed(0, WaveFunctionCollapse.NORTH, 1): print("allow leaked"); fails += 1
	if not dir.is_allowed(1, WaveFunctionCollapse.WEST, 0): print("no mirror"); fails += 1
	if dir.generate(Vector2i(2, 1), 1) != PackedInt32Array([0, 1]):
		print("directional rule not honoured"); fails += 1
	# nothing may sit east of anything, so a 2-wide grid cannot be filled
	if not dir.generate(Vector2i(2, 2), 1).is_empty():
		print("unsatisfiable rules should give an empty array"); fails += 1

	# clear()
	wfc.clear()
	if wfc.is_allowed(Tile.SAND, WaveFunctionCollapse.EAST, Tile.SAND):
		print("clear left rules behind"); fails += 1

	print("GDFAILS=", fails)
	quit()
