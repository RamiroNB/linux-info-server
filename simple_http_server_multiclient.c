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
#include <sys/sysinfo.h>
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
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	strftime(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>Date/Time: %c</h3>", tm_info);
}

void get_uptime(char *buffer, size_t buf_size)
{
	struct sysinfo info;
	sysinfo(&info);
	snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>Uptime: %.2f seconds</h3>", info.uptime);
}

void get_processor_info(char *buffer, size_t buf_size)
{
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strncmp(line, "model name", 11) == 0)
			{
				strncat(buffer + strlen(buffer), "<h3>Processor: ", buf_size - strlen(buffer) - 1);
				strncat(buffer + strlen(buffer), strchr(line, ':') + 2, buf_size - strlen(buffer) - 1);
				strncat(buffer + strlen(buffer), "</h3>", buf_size - strlen(buffer) - 1);
				break;
			}
		}
		fclose(fp);
	}
}

void get_memory_usage(char *buffer, size_t buf_size)
{
	struct sysinfo info;
	sysinfo(&info);
	snprintf(buffer + strlen(buffer), buf_size - strlen(buffer), "<h3>Memory Usage: Total: %ld MB, Used: %ld MB</h3>",
			 info.totalram / (1024 * 1024), (info.totalram - info.freeram) / (1024 * 1024));
}

void get_kernel_version(char *buffer, size_t buf_size)
{
	FILE *fp = fopen("/proc/version", "r");
	if (fp)
	{
		char line[256];
		if (fgets(line, sizeof(line), fp))
		{
			strncat(buffer + strlen(buffer), "<h3>Kernel Version: ", buf_size - strlen(buffer) - 1);
			strncat(buffer + strlen(buffer), strchr(line, ':') + 2, buf_size - strlen(buffer) - 1);
			strncat(buffer + strlen(buffer), "</h3>", buf_size - strlen(buffer) - 1);
		}
		fclose(fp);
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
	strncat(buffer, "<h3>Disk Units</h3><ul>", buf_size - strlen(buffer) - 1);
	FILE *fp = popen("df -h", "r");
	char line[256];
	while (fgets(line, sizeof(line), fp))
	{
		strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
		strncat(buffer, line, buf_size - strlen(buffer) - 1);
		strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
	}
	pclose(fp);
	strncat(buffer, "</ul><br>", buf_size - strlen(buffer) - 1);
}

void get_usb_info(char *buffer, size_t buf_size)
{
	strncat(buffer, "<h3>USB Devices</h3><ul>", buf_size - strlen(buffer) - 1);

	DIR *dir = opendir("/sys/bus/usb/devices");
	struct dirent *entry;

	if (!dir)
	{
		strncat(buffer, "Unable to open /sys/bus/usb/devices.<br>", buf_size - strlen(buffer) - 1);
		return;
	}

	struct stat st;
	char path[256];

	while ((entry = readdir(dir)) != NULL)
	{
		snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s", entry->d_name);

		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
		{
			strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
			strncat(buffer, entry->d_name, buf_size - strlen(buffer) - 1);
			strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
		}
	}

	closedir(dir);
	strncat(buffer, "</ul><br>", buf_size - strlen(buffer) - 1);
}

void get_network_adapters(char *buffer, size_t buf_size)
{
	strncat(buffer, "<h3>Network Adapters</h3><ul>", buf_size - strlen(buffer) - 1);

	FILE *fp = fopen("/proc/net/dev", "r");
	if (fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			if (strchr(line, ':'))
			{
				char *iface = strtok(line, ":");
				if (iface)
				{
					strncat(buffer, "<li>", buf_size - strlen(buffer) - 1);
					strncat(buffer, iface, buf_size - strlen(buffer) - 1);
					strncat(buffer, " - IP: ", buf_size - strlen(buffer) - 1);
					char ip[64];
					snprintf(ip, sizeof(ip), "ip -o -4 addr show %s | awk '{print $4}'", iface);
					FILE *ip_fp = popen(ip, "r");
					char ip_addr[32];
					if (fgets(ip_addr, sizeof(ip_addr), ip_fp))
					{
						strncat(buffer, ip_addr, buf_size - strlen(buffer) - 1);
					}
					pclose(ip_fp);
					strncat(buffer, "</li>", buf_size - strlen(buffer) - 1);
				}
			}
		}
		fclose(fp);
	}

	strncat(buffer, "</ul><br>", buf_size - strlen(buffer) - 1);
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
