#include "engine.h"
#include "MultiPlayer/WebGui.h"
#include "MultiPlayer/Server.h"
#include "MultiPlayer/Client.h"
#include "imgui.h"
#include <cerrno>
#include <cstdlib>



#include <thread>
#include <fstream>
#include <sstream>
#include <enet/enet.h>

std::string read_file(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return "<html><body><p>Hello error, could not find file</p></body></html>";
	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}


namespace MultiPlayer {
	namespace {
		int readWebGuiPort()
		{
			const char* portEnv = std::getenv("MULTIPLAYER_WEBGUI_PORT");
			if (!portEnv || !*portEnv) {
				return 7776;
			}

			errno = 0;
			char* end = nullptr;
			long parsedPort = std::strtol(portEnv, &end, 10);
			if (errno != 0 || end == portEnv || *end != '\0' || parsedPort <= 0 || parsedPort > 65535) {
				return 7776;
			}

			return static_cast<int>(parsedPort);
		}
	}

	WebGui::WebGui(Engine* engine)
		: connect(false)
		  , address("127.0.0.1")
		  , port(7775)
		  , engine(engine)
	{


		CROW_ROUTE(app, "/")([](){
				return read_file("src/MultiPlayer/WebGui.html");
				});

		CROW_ROUTE(app, "/multiplayer/connect").methods(crow::HTTPMethod::POST)
			([this, engine](const crow::request& req) {
			 auto params = req.get_body_params();

			 auto host = params.get("host");
			 auto port = params.get("port");

			 if (!host || !port) {
			 return crow::response(400, "missing host or port");
			 }

			 std::stringstream ss;

			client = std::make_shared<Client>(engine->getRegistry());
			engine->registerClient(client);

			// TODO:Investigate how the client is closed
			client->connect(host, std::atoi(port));
			if(client->isConnected()) {
			 ss << "connected to ... <" << host << ">:<" << port << ">";
			} else {
			  ss << "failed to connect to ... <" << host << ">:<" << port << ">";
		        }

			 return crow::response{ss.str()};
			 });
		serverThread = std::thread([this](){
				app.port(port).multithreaded().run();
				});
	}

	WebGui::~WebGui() {}

	void WebGui::Render() {}
}
