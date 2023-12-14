import anvil

region = anvil.Region.from_file('/home/collin/Downloads/hogwarts/region/r.0.0.mca')

# You can also provide the region file name instead of the object
chunk = anvil.Chunk.from_region(region, 0, 0)

# If `section` is not provided, will get it from the y coords
# and assume it's global
block = chunk.get_block(10, 20, 2)

print(block) # <Block(minecraft:air)>
print(block.id) # air
print(block.properties) # {}
