const api = require("../../client_libs/js/api");
const maze = require("./maze.js");


const width = 80;
const height = 80;


const moves = [[0,1], [0,-1], [1,0], [-1,0]];
let visited = [];
let next = [];

const movesPerSecond = 300;

function tick() {
    return new Promise(resolve => setTimeout(resolve, 1000/movesPerSecond));
}

function hasNotBeenVisited(pos) {
    for(visit of visited) {
        if(visit[0]==pos[0] && visit[1] == pos[1]) {
            return false;
        }
    }
    return true;
}

async function bfs(pos, target, mazeGrid) {
    await tick();
    await maze.drawVisit(pos);
    if(pos[0] == target[0] && pos[1] == target[1]) {
        return;
    }

    for(const move of moves) {
        const moved = [move[0]+pos[0], move[1]+pos[1]];

        const notVisited = hasNotBeenVisited(moved);
        const inBoundsX = moved[0] >= 0 && moved[0] < width;
        const inBoundsY = moved[1] >= 0 && moved[1] < height;
        const inBounds = inBoundsX && inBoundsY;

        // guard against out of bounds access
        const isNotWall = inBounds && !mazeGrid[moved[1]][moved[0]];

        const goodMove = notVisited && inBounds && isNotWall;
        if(goodMove) {
            next.push(moved);
            visited.push(moved);
        }
    }

    await bfs(next.shift(),
              target,
              mazeGrid);
}


async function solve(mazeGrid) {
    const start = [0,1];
    visited = [];
    next = [];
    await bfs(start, [width-2, height-1], mazeGrid);
    await (new Promise(resolve => setTimeout(resolve, 3)));
    await maze.clear(width, height);
}

async function main() {
    await api.init();
    while(true) {
        await maze.clear(width, height);
        mazeGrid = maze.generate(width, height);
        await maze.render(mazeGrid, width, height);
        await solve(mazeGrid);
    }
}

main();
