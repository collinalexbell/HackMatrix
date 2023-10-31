const api = require("../../client_libs/js/api");

function generate(width, height) {
    const grid = new Array(height);
    for (let i = 0; i < height; i++) {
        grid[i] = new Array(width).fill(true);
    }

    function carvePassages(x, y) {
        const directions = [[0, 2], [0, -2], [2, 0], [-2, 0]];
        const randomDirections = directions.sort(() => Math.random() - 0.5);

        for (const [dx, dy] of randomDirections) {
            const nx = x + dx;
            const ny = y + dy;

            if (nx >= 1 && nx < width - 1 && ny >= 1 && ny < height - 1 && grid[ny][nx]) {
                grid[ny][nx] = false;
                grid[y + dy / 2][x + dx / 2] = false;
                carvePassages(nx, ny);
            }
        }
    }

    const startX = 1;
    const startY = 1;
    const endX = width - 2;
    const endY = height - 2;

    grid[startY][startX] = false;
    grid[startY][0] = false;
    grid[endY][endX] = false;
    grid[endY+1][endX] = false;
    grid[endY][endX-1] = false;
    carvePassages(startX, startY);

    // Return the boolean grid
    return grid;
}


const yOffset = 10;
const xOffset = 0;
const zOffset = 0;
const wallH = 1;
const passageW = 1;
const passageH = 1;
async function render(maze, width, height) {
    cubes = [];
    for(x=0; x<width*passageW; x++) {
        for(y=0;y<height*passageH; y++) {
            await api.addCube(x+xOffset,y+yOffset,zOffset,4);
        }
    }
    for(y = 0; y<height; y++) {
        for(x= 0; x<width; x++) {
            if(maze[y][x]) {
                for(dx = 0; dx < passageW; dx++) {
                    for(dy=0; dy < passageH; dy++) {
                        for(h=1; h<=wallH; h++) {
                            await api.addCube(xOffset+x*passageW+dx,
                                              yOffset+y*passageH+dy,
                                              zOffset+1,
                                              0);
                        }
                    }
                }
            }
        }
    }
}

function tick() {
    return new Promise(resolve => setTimeout(resolve, 5));
}
async function clear(width,height){
    for(x=xOffset; x<xOffset+(width*passageW); x++) {
        for(y=yOffset; y<yOffset+(width*passageW); y=y+1) {
            await api.clearBox(x,y,zOffset,
                               x+1,
                               y+1,
                               zOffset+wallH+1);
        }
        await tick();
    }
}

async function drawVisit(pos) {
    const x = pos[0];
    const y = pos[1];

    await api.addCube(x+xOffset,y+yOffset,zOffset+1,6);
}


module.exports = {generate, render, drawVisit, clear};
