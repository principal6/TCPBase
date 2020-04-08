#include "TCPServer.h"
#include <thread>

int main()
{
	CTCPServer Server{};
	std::thread thrReceive{
		[&]()
		{
			while (true)
			{
				if (Server.IsClosing()) break;

				Server.ReceiveAll();
			}
		}
	};

	char Command[1024]{};
	while (true)
	{
		if (Server.IsClosing()) break;

		fgets(Command, 1024, stdin);
		for (auto& Ch : Command)
		{
			if (Ch == '\n') Ch = 0;
		}
		if (strncmp(Command, "/", 1) == 0)
		{
			if (strncmp(Command, "/?", 2) == 0)
			{
				printf("  /?      Display help\n");
				printf("  /info   Display socket information\n");
				printf("  /quit   Close the server and terminate the program\n");
				printf("\n");
			}
			else if (strncmp(Command, "/info", 4) == 0)
			{
				Server.DisplayInfo();
			}
			else if (strncmp(Command, "/quit", 4) == 0)
			{
				Server.Close();
			}
			else
			{
				printf("  There is no such command. Please type /? for help.\n");
			}
			continue;
		}
		Server.SendToAll(Command);
	}

	thrReceive.join();
	return 0;
}