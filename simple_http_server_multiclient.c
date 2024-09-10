#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>

#define BUFLEN 8192
#define PORT 5000
#define MAX_PROCESSES 100
#define PROCESSES_PER_PAGE 10

// fix uptmie, cpuinfo, kernal version
void die(char *s)
{
	perror(s);
	exit(1);
}

void get_current_datetime(char *buffer, size_t buf_size)
{
	// read from /proc/datetime
	FILE *fp = fopen("/proc/datetime", "r");
	if (fp)
	{
		char line[256];
		if (fgets(line, sizeof(line), fp))
		{
			strncat(buffer, "<h3>Current Date/Time: ", buf_size - strlen(buffer) - 1);
			strncat(buffer, line, buf_size - strlen(buffer) - 1);
			strncat(buffer, "</h3>", buf_size - strlen(buffer) - 1);
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>Current Date/Time: N/A</h3>", buf_size - strlen(buffer) - 1);
	}
}

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
		snprintf(buffer, len, "<h3>Uptime: %.2f seconds</h3>", uptime);
		fclose(fp);
	}
}

void get_processor_info(char *buffer, size_t buf_size)
{
	// read from /proc/cpuinfo

	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, "model name"))
			{
				strncat(buffer, "<h3>Processor: ", buf_size - strlen(buffer) - 1);
				strncat(buffer, strchr(line, ':') + 2, buf_size - strlen(buffer) - 1);
				strncat(buffer, "</h3>", buf_size - strlen(buffer) - 1);
				break;
			}
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>Processor: N/A</h3>", buf_size - strlen(buffer) - 1);
	}

	// cpu usage
	FILE *fp1 = fopen("/proc/stat", "r");

	if (fp1)
	{
		char line[256];
		long user = 0, nice = 0, system = 0, idle = 0;
		while (fgets(line, sizeof(line), fp1))
		{
			if (sscanf(line, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle) == 4)
			{
				break;
			}
		}
		long total = user + nice + system + idle;
		long cpu_usage = 100 * (total - idle) / total;
		snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>CPU Usage: %ld%%</h3>", cpu_usage);
		fclose(fp1);
	}
	else
	{
		snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>CPU Usage: N/A</h3>");
	}
}

void get_memory_usage(char *buffer, size_t buf_size)
{
	// read from /proc/meminfo
	FILE *fp = fopen("/proc/meminfo", "r");
	if (fp)
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
		snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>Memory Usage: Total: %ld MB, Used: %ld MB</h3>",
				 total_mem / 1024, (total_mem - free_mem) / 1024);
		fclose(fp);
	}
	else
	{
		snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>Memory Usage: N/A</h3>");
	}
}

void get_kernel_version(char *buffer, size_t buf_size)
{
	// read from /proc/version
	FILE *fp = fopen("/proc/version", "r");
	if (fp)
	{
		char line[256];
		if (fgets(line, sizeof(line), fp))
		{
			strncat(buffer, "<h3>Kernel Version: ", buf_size - strlen(buffer) - 1);
			strncat(buffer, line, buf_size - strlen(buffer) - 1);
			strncat(buffer, "</h3>", buf_size - strlen(buffer) - 1);
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>Kernel Version: N/A</h3>", buf_size - strlen(buffer) - 1);
	}
}

void get_processes(char *buffer, size_t buf_size, int page)
{
	strncat(buffer, "<h3>Running Processes</h3><ul>", buf_size - strlen(buffer) - 1);
	DIR *dir = opendir("/proc");
	struct dirent *entry;

	if (!dir)
	{
		strncat(buffer, "Unable to open /proc.<br>", buf_size - strlen(buffer) - 1);
		return;
	}

	struct stat st;
	int count = 0;
	int start_index = (page - 1) * PROCESSES_PER_PAGE;

	while ((entry = readdir(dir)) != NULL)
	{
		if (isdigit(entry->d_name[0]))
		{
			if (count >= start_index && count < start_index + PROCESSES_PER_PAGE)
			{
				char path[256];
				snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
				FILE *fp = fopen(path, "r");
				if (fp)
				{
					char cmdline[256];
					fgets(cmdline, sizeof(cmdline), fp);
					fclose(fp);
					if (strlen(cmdline) > 0)
					{
						strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
						strncat(buffer, entry->d_name, buf_size - strlen(buffer) - 1); // PID
						strncat(buffer, ": ", buf_size - strlen(buffer) - 1);
						strncat(buffer, cmdline, buf_size - strlen(buffer) - 1); // Command
						strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
					}
				}
			}
			count++;
		}
	}

	closedir(dir);
	strncat(buffer, "</ul><br>", buf_size - strlen(buffer) - 1);

	// Pagination links
	snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<p>");
	if (page > 1)
	{
		snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<a href=\"/?page=%d\">Previous</a> ", page - 1);
	}
	snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<a href=\"/?page=%d\">Next</a>", page + 1);
	snprintf(buffer + strlen(buffer), buf_size - strlen(buffer) - 1, "</p>");
}

void get_disks(char *buffer, size_t buf_size)
{
	// read from /proc/partitions
	FILE *fp = fopen("/proc/partitions", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, "sd"))
			{
				strncat(buffer, "<h3>Disks</h3><ul>", buf_size - strlen(buffer) - 1);
				strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
				strncat(buffer, line, buf_size - strlen(buffer) - 1);
				strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
			}
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>Disks: N/A</h3>", buf_size - strlen(buffer) - 1);
	}
}

void get_usb_info(char *buffer, size_t buf_size)
{
	// read from /proc/bus/usb/devices
	FILE *fp = fopen("/proc/bus/usb/devices", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, "Product="))
			{
				strncat(buffer, "<h3>USB Devices</h3><ul>", buf_size - strlen(buffer) - 1);
				strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
				strncat(buffer, line, buf_size - strlen(buffer) - 1);
				strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
			}
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>USB Devices: N/A</h3>", buf_size - strlen(buffer) - 1);
	}
}

void get_network_adapters(char *buffer, size_t buf_size)
{
	// read from /proc/net/dev
	FILE *fp = fopen("/proc/net/dev", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strstr(line, ":"))
			{
				strncat(buffer, "<h3>Network Adapters</h3><ul>", buf_size - strlen(buffer) - 1);
				strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
				strncat(buffer, line, buf_size - strlen(buffer) - 1);
				strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
			}
		}
		fclose(fp);
	}
	else
	{
		strncat(buffer, "<h3>Network Adapters: N/A</h3>", buf_size - strlen(buffer) - 1);
	}
}

int main(void)
{
	struct sockaddr_in si_me, si_other;
	int s, conn, slen = sizeof(si_other), recv_len;
	char buf[BUFLEN];
	char http_ok[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
	char page[BUFLEN * 2]; // Increase buffer size for larger pages

	// Create a TCP socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		die("socket");

	// Zero out the structure
	memset((char *)&si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind socket to port
	if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
		die("bind");

	// Allow 10 requests to queue up
	if (listen(s, 10) == -1)
		die("listen");

	// Keep listening for data
	while (1)
	{
		printf("Waiting for connection...\n");
		fflush(stdout);

		conn = accept(s, (struct sockaddr *)&si_other, &slen);
		if (conn < 0)
			die("accept");

		printf("Client connected: %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

		memset(buf, 0, sizeof(buf));
		recv_len = read(conn, buf, BUFLEN);
		if (recv_len < 0)
			die("read");

		printf("Received request:\n%s\n", buf);

		// Parse the page number from the request
		int page_number = 1;
		char *page_param = strstr(buf, "page=");
		if (page_param)
		{
			sscanf(page_param, "page=%d", &page_number);
		}

		// Clear the page buffer
		memset(page, 0, sizeof(page));

		// Generate the HTML page with system information
		snprintf(page, sizeof(page), "<html><head><title>System Information</title></head><body>");
		strncat(page, "<h1>System Information</h1>", sizeof(page) - strlen(page) - 1);
		get_current_datetime(page, sizeof(page));
		get_uptime(page, sizeof(page));
		get_processor_info(page, sizeof(page));
		get_memory_usage(page, sizeof(page));
		get_kernel_version(page, sizeof(page));
		get_processes(page, sizeof(page), page_number);
		get_disks(page, sizeof(page));
		get_usb_info(page, sizeof(page));
		get_network_adapters(page, sizeof(page));
		strncat(page, "</body></html>", sizeof(page) - strlen(page) - 1);

		printf("Sending response:\n%s\n", page);

		if (write(conn, http_ok, strlen(http_ok)) < 0)
			die("write");

		if (write(conn, page, strlen(page)) < 0)
			die("write");

		close(conn);
	}

	close(s);
	return 0;
}
