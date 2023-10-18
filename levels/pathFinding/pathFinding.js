const api = require("../../client-libs/js/api");

async function main() {
    await api.init();
    cubes = [];
    y = 20;
    for(z = 0; z<50; z++) {
        for(x= 0; x<50; x++) {
            await api.addCube(x,y,z,5);
            console.log("x:" + x+ ", y:" + y + ", z:" + z);
        }
    }
}
main();
