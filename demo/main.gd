extends Node

enum Tile { WATER, SAND, GRASS }

const GLYPHS := ["~", ".", "#"]
const SIZE := Vector2i(24, 12)


func _ready() -> void:
	var wfc := WaveFunctionCollapse.new()

	# Water and grass may never touch, so there is always a beach between them.
	wfc.allow_all(Tile.WATER, Tile.WATER)
	wfc.allow_all(Tile.WATER, Tile.SAND)
	wfc.allow_all(Tile.SAND, Tile.SAND)
	wfc.allow_all(Tile.SAND, Tile.GRASS)
	wfc.allow_all(Tile.GRASS, Tile.GRASS)
	wfc.set_weight(Tile.WATER, 1.5)

	var grid := wfc.generate(SIZE, 1234)
	if grid.is_empty():
		push_error("contradiction; retry with another seed")
		return

	for y in SIZE.y:
		var row := ""
		for x in SIZE.x:
			row += GLYPHS[grid[y * SIZE.x + x]]
		print(row)
