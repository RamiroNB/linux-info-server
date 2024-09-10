#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define BUFLEN 1024
#define PORT 8000

char http_ok[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";
char http_error[] = "HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";

// Function to read from /proc files
void read_file(const char *path, char *buffer, size_t len)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
	{
		snprintf(buffer, len, "N/A");
	}
	else
	{
		fgets(buffer, len, fp);
		fclose(fp);
	}
}

// Function to get system uptime
void get_uptime(char *buffer, size_t len)
{
	FILE *fp = fopen("/proc/uptime", "r");
	if (fp == NULL)
	{
		snprintf(buffer, len, "N/A");
	}
	else
	{
		double uptime;
		fscanf(fp, "%lf", &uptime);
		snprintf(buffer, len, "%.2f seconds", uptime);
		fclose(fp);
	}
}

// Function to get processor model
void get_cpuinfo(char *buffer, size_t len)
{
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp != NULL)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, "model name"))
			{
				strcpy(buffer, strchr(line, ':') + 2);
				break;
			}
		}
		fclose(fp);
	}
	else
	{
		snprintf(buffer, len, "N/A");
	}
}

// Function to get system memory usage
void get_memory_info(char *buffer, size_t len)
{
	FILE *fp = fopen("/proc/meminfo", "r");
	if (fp != NULL)
	{
		char line[256];
		long total_mem = 0, free_mem = 0;
		while (fgets(line, sizeof(line), fp))
		{
			if (sscanf(line, "MemTotal: %ld", &total_mem) == 1 ||
				sscanf(line, "MemFree: %ld", &free_mem) == 1)
			{
				if (total_mem && free_mem)
					break;
			}
		}
		snprintf(buffer, len, "Total: %ld MB, Used: %ld MB", total_mem / 1024, (total_mem - free_mem) / 1024);
		fclose(fp);
	}
	else
	{
		snprintf(buffer, len, "N/A");
	}
}

// Function to serve the dynamically generated page
void serve_dynamic_page(int conn)
{
	char buffer[BUFLEN], uptime[128], cpuinfo[256], memory_info[256], version[128];
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	// Gather data
	get_uptime(uptime, sizeof(uptime));
	get_cpuinfo(cpuinfo, sizeof(cpuinfo));
	get_memory_info(memory_info, sizeof(memory_info));
	read_file("/proc/version", version, sizeof(version));

	// Build the HTML page dynamically
	snprintf(buffer, sizeof(buffer),
			 "<html><head><title>System Info</title></head>"
			 "<body>"
			 "<h1>System Information</h1>"
			 "<p><b>Date/Time:</b> %s</p>"
			 "<p><b>Uptime:</b> %s</p>"
			 "<p><b>Processor:</b> %s</p>"
			 "<p><b>Memory Usage:</b> %s</p>"
			 "<p><b>Kernel Version:</b> %s</p>"
			 "</body></html>",
			 asctime(t), uptime, cpuinfo, memory_info, version);

	// Send response
	write(conn, http_ok, strlen(http_ok));
	write(conn, buffer, strlen(buffer));
}

int main(void)
{
	struct sockaddr_in si_me, si_other;
	int s, conn, slen = sizeof(si_other), recv_len;
	char buf[BUFLEN];

	// Create TCP socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		perror("socket"), exit(1);

	// Setup address structure
	memset((char *)&si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind to port
	if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
		perror("bind"), exit(1);

	// Listen for incoming connections
	if (listen(s, 10) == -1)
		perror("listen"), exit(1);

	// Keep listening for incoming connections
	while (1)
	{
		memset(buf, 0, sizeof(buf));
		printf("Waiting for a connection...\n");
		fflush(stdout);

		// Accept connection
		conn = accept(s, (struct sockaddr *)&si_other, &slen);
		if (conn < 0)
			perror("accept"), exit(1);

		printf("Client connected: %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

		// Read request (simple HTTP GET parsing)
		recv_len = read(conn, buf, BUFLEN);
		if (recv_len < 0)
			perror("read"), exit(1);

		printf("Received request: %s\n", buf);

		if (strstr(buf, "GET"))
		{
			// Serve dynamic page
			serve_dynamic_page(conn);
		}
		else
		{
			// Handle bad request
			write(conn, http_error, strlen(http_error));
		}

		// Close the connection
		close(conn);
	}

	close(s);
	return 0;
}
