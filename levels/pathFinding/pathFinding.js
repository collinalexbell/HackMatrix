const api = require("../../client-libs/js/api");

async function main() {
    cubes = [];
    for(j = 0; j<50; j++) {
        for(i = 0; i<50; i++) {
            await api.addCube(i,15,j,3);
        }
    }
}
main();
