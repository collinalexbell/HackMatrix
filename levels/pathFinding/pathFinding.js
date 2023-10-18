const api = require("../../client-libs/js/api");
const randomizedPrimAlgo = require("./randomizedPrimAlgo.js");


async function main() {
    const width = 80;
    const height = 80;
    const passageW = 1;
    const passageH = 1;
    const wallH = 1;
    const y = 65;
    maze = randomizedPrimAlgo.generateMaze(width, height);
    await api.init();
    cubes = [];
    for(x=0; x<width*passageW; x++) {
        for(z=0;z<height*passageH; z++) {
            await api.addCube(x,y+wallH,z,4);
        }
    }
    for(z = 0; z<height; z++) {
        for(x= 0; x<width; x++) {
            if(maze[x][z]) {
                for(dx = 0; dx < passageW; dx++) {
                    for(dz=0; dz < passageH; dz++) {
                        for(h=0; h<=wallH; h++) {
                            await api.addCube(x*passageW+dx,y+h,z*passageH+dz,0);
                        }
                    }
                }
            }
        }
    }
}
main();
