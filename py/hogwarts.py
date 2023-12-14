import anvil
from hackMatrix.api import add_cube

region = anvil.Region.from_file('/home/collin/Downloads/hogwarts/region/r.0.0.mca')

# You can also provide the region file name instead of the object

# If `section` is not provided, will get it from the y coords
# and assume it's global
blockCounts = {}

chunk = anvil.Chunk.from_region(region, 303,0 )
with open("hogwarts.0.0.save", "w+") as f:
    for chunkX in range(32):
        for chunkZ in range(32):
            chunk = anvil.Chunk.from_region(region, chunkX, chunkZ)
            for x in range(16):
                for y in range(256):
                    for z in range(16):
                        id = -1
                        block = chunk.get_block(x,y,z)
                        xAbs = x+chunkX*16
                        yAbs = y
                        zAbs = z+chunkZ*16
                       # if(block.id not in blockCounts):
                       #     blockCounts[block.id] = 0
                       # else:
                       #     blockCounts[block.id] += 1
                        if(block.id == 'stone'):
                            id = 6
                        if(block.id.find("concrete") != -1):
                            id = 0
                        if(block.id == 'grass'):
                            id = 3
                        if(block.id == 'netherrack'):
                            id = 1
                        else:
                            pass
                        if(id != -1):
                            line = f"{xAbs},{yAbs},{zAbs},{id}\n"
                            f.write(line)

