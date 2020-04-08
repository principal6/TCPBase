#include "TCPClient.h"
#include <thread>

int main()
{
	CTCPClient Client{};
	Client.Connect("192.168.219.200", 9999);

	std::thread thrReceive{
		[&]()
		{
			while (true)
			{
				if (Client.IsLeaving()) break;
				if (Client.Receive())
				{
					std::cout << Client.GetBuffer() << std::endl;
				}
			}
		}
	};

	char Command[1024]{};
	while (true)
	{
		if (Client.IsLeaving()) break;

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
				printf("  /quit   Terminate the program\n");
				printf("\n");
			}
			else if (strncmp(Command, "/info", 4) == 0)
			{
				Client.DisplayInfo();
			}
			else if (strncmp(Command, "/quit", 4) == 0)
			{
				Client.Leave();
			}
			else
			{
				printf("  There is no such command. Please type /? for help.\n");
			}
			continue;
		}
		Client.Send(Command);
	}

	thrReceive.join();
	return 0;
}