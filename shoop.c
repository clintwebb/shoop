// shoop is a simple single-threaded command-line based HTTP service.
// by Clinton Webb (webb.clint@gmail.com)
// March 31, 2010.

/*

Copyright, Clinton Webb, 2010.
Released under GPL licence.
http://www.gnu.org/licenses/licenses.html#GPL

*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <dirent.h>


char *path = NULL;
char *indexfile = NULL;
char *hexchars = "0123456789abcdef";


// pointer points to a string with hex ascii values;
char hexchar(char *ptr) 
{
	char c = 0;
	char x,y;
	
	x = ptr[0];
	y = ptr[1];
	
	if (x >= '0' && x <= '9') { c = (x - '0') << 4; }
	else if (x >= 'a' && x <= 'f') { c = (10 + (x - 'a')) << 4; }
	else if (x >= 'A' && x <= 'F') { c = (10 + (x - 'A')) << 4; }
	
	if (y >= '0' && y <= '9') { c += (y - '0'); }
	else if (y >= 'a' && y <= 'f') { c += (y - 'a') + 10; }
	else if (y >= 'A' && y <= 'F') { c += (y - 'A') + 10; }
	
	return (c);
}

void process_conn(int conn) 
{
	char buffer[4096 * 4];
	int length;
	FILE *fp;
	char fullpath[4096*2];
	int fi;
	int sent;
	char outstr[4096*2];
	int oi;
	long flen;
	long fleft;
	long ff;
	DIR *dir;
	struct dirent *dentry;
	char *pp;
	char link[4096];
	char *lp;
	
	length = recv(conn, buffer, sizeof(buffer), 0);
	assert(length < sizeof(buffer));
	buffer[length] = 0;
	
	// parse the first line of the buffer, to determine the path.
	char *pot = buffer;
	
	// skip the first word.
	while (*pot != 0 && *pot != ' ') { pot ++; }
	
	if (*pot == 0) {
		// need to return a 503 error.
		assert(0);
	}
	else {

		// start off with the basepath in the fullpath.
		if (path == NULL) {
			sprintf(fullpath, ".");
			fi = 1;
		}
		else {
			sprintf(fullpath, "%s", path);
			fi = strlen(path);
		}
		
		assert(*pot == ' ');
		pot ++;
		while (*pot != 0 && *pot != ' ') { 
			if (*pot == '%') {
				pot ++;
				fullpath[fi] = hexchar(pot);
				pot ++;
			}
			else if (*pot == '.' && pot[1] == '.') {
				pot++;
			}
			else {
				fullpath[fi] = *pot;
			}
			fi++;
			pot++;
		}
		fullpath[fi] = 0;

		assert(fi > 0);
		
		if (fullpath[fi-1] == '/' && indexfile == NULL) {
			// specific file not requested... we need to generate a file listing.
			printf("creating index for: %s\n", fullpath);
			dir = opendir(fullpath);
			
			if (dir == NULL) {
				sprintf(outstr, "HTTP/1.0 404 OK\r\n\r\nFile not found.\r\n");
				oi = strlen(outstr);
				sent = send(conn, outstr, oi, 0);
				assert(oi == sent);
			}
			else {

				sprintf(outstr, "HTTP/1.0 200 OK\r\n\r\n<html><head><title>Directory Listing</title></head><body><ul><li><a href=\"../\">../</a></li>");
				oi = strlen(outstr);
				sent = send(conn, outstr, oi, 0);
				assert(oi == sent);
				while ((dentry = readdir(dir))) {

					if (dentry->d_name[0] != '.') {
					
						// need to URL encode the link.
						lp = link;
						pp = dentry->d_name;
						while (*pp != 0) {
							if (*pp == ' ') {
								*lp = '%'; lp ++;
								*lp = hexchars[*pp >> 4]; lp ++;
								*lp = hexchars[*pp & 0xf]; lp ++;
							}
							else if (*pp == '#' || *pp == '[' || *pp == ']') {
								*lp = '%'; lp ++;
								*lp = hexchars[*pp >> 4]; lp ++;
								*lp = hexchars[*pp & 0xf]; lp ++;
							}
							else {
								*lp = *pp; lp ++;
							}
							pp ++;
						}
						*lp = 0;
						
						if (dentry->d_type == DT_DIR || dentry->d_type == DT_LNK) {
							sprintf(outstr, "<li><a href=\"%s/\">%s/</a></li>\r\n", link, dentry->d_name);
						}
						else {
							sprintf(outstr, "<li><a href=\"%s\">%s</a></li>\r\n", link, dentry->d_name);
						}
						oi = strlen(outstr);
						sent = send(conn, outstr, oi, 0);
						assert(oi == sent);
					}
				}
				
				sprintf(outstr, "</ul></body>");
				oi = strlen(outstr);
				sent = send(conn, outstr, oi, 0);
				assert(oi == sent);
				
				closedir(dir);
			}
		}
		else {
			if (fullpath[fi-1] == '/') 
				strcat(fullpath, indexfile);
		
			printf("Checking for file: %s\n", fullpath);
			
			fp = fopen(fullpath, "r");
			if (fp == NULL) {
				// send error
				sprintf(outstr, "HTTP/1.0 404 OK\r\n\r\nFile not found.\r\n");
				oi = strlen(outstr);
				
				sent = send(conn, outstr, oi, 0);
				assert(oi == sent);
			}
			else {

				// get length of the file.
				fseek(fp, 0, SEEK_END);
				flen = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				
				sprintf(outstr, "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", flen);
				oi = strlen(outstr);
				sent = send(conn, outstr, oi, 0);
				assert(oi == sent);
				
				fleft = flen;
				while (fleft > 0) {
					
					if (fleft > sizeof(buffer)) { ff = sizeof(buffer); }
					else { ff = fleft; }
					
					fread(buffer, ff, 1, fp);
					sent = send(conn, buffer, ff, 0);
					if (sent == ff) { fleft -= ff; }
					else { 
						printf("Connection closed.\n");
						fleft = 0; 
					}
				}
				
				fclose(fp);
			}
		}
	}
	close(conn);
}

void usage(void)
{
	
	printf("shoop - command-line webserver\n\n");
	printf("  -p port\n");
	printf("  -b basedir\n");
	printf("  -i indexfile\n\n");
	
	exit(1);
}


int main(int argc, char **argv) 
{
	int c;
	int port = 8080;
	int listener = 0;
	int connection;
	struct sockaddr_in server_address; 
	int reuse_addr = 1;
	
	
	// get the command-line parameters.
	while ((c = getopt(argc, argv, "p:b:i:h")) != -1) {
		switch (c) {
			case 'p':
				port = atoi(optarg);
				break;
				
			case 'b':
				path = strdup(optarg);
				break;
				
			case 'i':
				indexfile = strdup(optarg);
				break;
		
			case 'h':
				usage();
				break;
		}
	}
	
	
	// load the mime-types files.
	
	
	// start the listener.
	// Obtain a file descriptor for our "listening" socket 
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener < 0) {
		fprintf(stderr, "Unable to create listening socket.");
		return(1);
	}
	assert(listener > 0);
	
	// We need to set the SO_REUSEADDR option on the socket so that we can 
	// avoid the annoying TIME_WAIT problems that cause a listening socket to 
	// be held for a few minutes even after the application has closed.
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) != 0) {
		fprintf(stderr, "unable to set socket options.\n");
		return(1);
	}
	
	// bind the listening socket to an actual port.  we listen on this port 
	// for new connections to come in.
	memset((char *) &server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY; //htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	if (bind(listener, (struct sockaddr *) &server_address, sizeof(server_address)) != 0 ) {
		close(listener);
		listener = 0;
		fprintf(stderr, "Listener is unable to bind to port %d", port);
		return(1);
	}
	
	// Set up queue for incoming connections.
	if (listen(listener, 5) != 0) {
		fprintf(stderr, "unable to listen on port %d", port);
		close(listener);
		listener = 0;
		return(1);
	}
	
	printf("Listening on port %d\n", port);
	
	
	// accept loop.
	// We have a new connection, we need to accept it.
	// This will continue looping when a new connection is received, until the application is interrupted.
	while ((connection = accept(listener, NULL, NULL) )) {
		printf("Got a connection: %d\n", connection);
		process_conn(connection);
	}

	close(listener);

	return(0);
}


