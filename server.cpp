#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>

#include <algorithm>
#include <numeric>
#include <deque>
#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <cstdio>
#include <string>
#include <future>
#include <mutex>

#define PORT    3490
#define MAXMSG  512

using namespace std;

const string DEFAULT_EMAIL = "stetorvs@gmail.com";
constexpr time_t TIMEOUT_THRESHOLD = 3;

// Concurrency
mutex write_mutex;
deque<future<void>> futures;

// Group member data
map<string, vector<string>> group_2_members = {
  {"The Hosts", {"victor"}},
};

// Timeout
map<string, struct timespec> timeout; /* TODO: replace with C++ chrono */

int make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  // Reuse socket if it is in a time-out state (prevents bind: Address already in use error)
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
}

time_t diff_currtime(struct timespec& base)
{
  struct timespec currtime;
  clock_gettime(CLOCK_REALTIME, &currtime);
  return difftime(currtime.tv_sec, base.tv_sec);
}

int reply_to_client (int filedes, string msg)
{
  int msg_size = msg.length() + 1;
  int nbytes = write(filedes, msg.c_str(), msg_size);
  fprintf (stderr, "Server sent message: %swith nybtes successful: %d\n", msg.c_str(), nbytes);
  return nbytes >= 0;
}

map<string, string> get_member_2_group(const map<string, vector<string>>& group_2_members_map)
{
  map<string, string> member_2_group;
  for (auto it : group_2_members_map) {
    for (auto member : it.second) {
      member_2_group.emplace(member, it.first);
    }
  }
  return member_2_group;
}

string get_response(const string& question, const string& ans, bool &correct)
{
  string response = "Invalid question\n";

  map<string, string> question_2_solution = {{"debug_msg", "fire"}, {"notes", "grebe"}, {"undo", "zztop"},
      {"cheating", "hacked"}, {"x", "markets"}, {"difference", "glidergun"}, {"mines", "innovation"},
      {"roman", "tokyoskytree"}, {"pigpen", "danbrown"}, {"training", "workshopshopper"},
      {"crash", "stevemeyers"}, {"work", "howtotrainyourdragon"},
      {"savings", "count"}};

  map<string, map<string, string>> question_2_hints;
  question_2_hints["debug_msg"] = {{"lpr0onblank", "lpr0 on _?\n"}};
  question_2_hints["undo"] = {{"savequitvim", "Replace this with the keyboard shortcut\n"},
      {"savequitvimnocolon", "Replace this with the keyboard shortcut\n"},
      {"quitvimnocolon", "With save, replace this with the keyboard shortcut\n"}};
  question_2_hints["cheating"] = {{"cheating", "This is not getting you ANYWHERE\n"},
      {"anywhere", "This is your last warning, or you will be REPORTED\n"},
      {"reported", "Is this really worth the EFFORT?\n"},
      {"effort", "Okay, you win. The answer is HACKED.\n"}};
  question_2_hints["x"] = {{"monopoly", "Good start, but that is just the beginning\n"}};
  question_2_hints["difference"] = {{"gameoflife", "Almost there, but please look at the other differences\n"},
      {"glider", "What's producing the gliders?\n"},
      {"gliders", "What's producing the gliders?\n"},
      {"gliderguns", "There's just one of them\n"},
      {"gospergliderguns", "Submit again without Gosper's name, and singular\n"},
      {"gosperglidergun", "Submit again without Gosper's name\n"}};
  question_2_hints["mines"] = {{"leadershipthisandcommunity", "What else would complete the trio?\n"}};
  question_2_hints["roman"] = {{"tallest", "Almost there...\n"},
      {"burjkhalifa", "Limit your search to just towers\n"}};
  question_2_hints["pigpen"] = {{"lostsymbolauthor", "He also wrote The Da Vinci Code\n"}};
  question_2_hints["training"] = {{"personnetwork", "Complete the loop between work and per\n"},
      {"shop", "Please submit both compound words, not just the common word of the two\n"}};
  question_2_hints["crash"] = {{"iwrote0321334876", "Almost there, you just need to know how to turn the number into a book\n"}};
  question_2_hints["work"] = {{"jonahhill", "Getting close...\n"},
      {"jaybaruchel", "Getting close...\n"},
      {"gerardbutler", "Getting close...\n"},
      {"jonahhill", "Getting close...\n"}};

  correct = false;

  auto it = question_2_solution.find(question);
  if (it != question_2_solution.end()) {
    if (ans == it->second) {
      correct = true;
      response = "Correct!\n";
    } else {
      auto hint_it = question_2_hints[question].find(ans);
      if (hint_it != question_2_hints[question].end()) {
        response = hint_it->second;
      } else {
        if (question == string("cheating")) {
          response = "please, no CHEATING\n";
        } else {
          response = "Sorry, incorrect answer\n";
        }
      }
    }
  }

  if (correct) {
    fprintf(stderr, "Correct answer!\n");
  } else {
    fprintf(stderr, "Incorrect answer\n");
  }

  return response;
}

void email(string msg, string subject, string to)
{
  if (msg.back() == '\n') {
    msg.pop_back();
  }
  if (subject.back() == '\n') {
    subject.pop_back();
  }
  string cmd = "echo '" + msg + "' | mail -s 'Puzzle: " + subject + "' " + to;
  system(cmd.c_str());
}

void update_scoreboard(string group, string question)
{
  lock_guard<mutex> write_lock(write_mutex);
  // TODO: update scoreboard text file
}

int read_from_client (int filedes)
{
  char buffer[MAXMSG];
  int nbytes;

  // static because I'm lazy
  static auto member_2_group_map = get_member_2_group(group_2_members);

  nbytes = read (filedes, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      /* Data read. */
      buffer[nbytes] = '\0';
      fprintf (stderr, "Server: got message: '%s'\n", buffer);
      string question, ans, user;
      stringstream ss;
      ss.str(string(buffer));
      ss >> question;
      ss >> ans;
      ss >> user;

      auto find_it = member_2_group_map.find(user);
      string response = string("Sorry, user ") + user + string(" does not belong to any team\n");
      time_t delta = (1 == timeout.count(user)) ? diff_currtime(timeout[user]) : (TIMEOUT_THRESHOLD + 1);
      if (delta < TIMEOUT_THRESHOLD) {
        response = string("Please wait ") + to_string(TIMEOUT_THRESHOLD - delta) + " second(s) before trying again\n";
      } else if (find_it != end(member_2_group_map)) {
        bool correct = false;
        response = get_response(question, ans, correct);
        if (!correct) {
          struct timespec currtime;
          clock_gettime(CLOCK_REALTIME, &currtime);
          timeout[user] = currtime;
        }
        string recipients = DEFAULT_EMAIL + " " + user + "@altera.com";
        if (correct) {
          auto group = find_it->second;
          auto group_members = group_2_members[group];
          recipients = accumulate(begin(group_members), end(group_members), recipients, [] (string ans, string in) { return ans + " " + in + "@altera.com";});

          futures.emplace_back(async(launch::async, [=] () { update_scoreboard(group, question); }));
        }
        futures.emplace_back(async(launch::async, [=] () { email(question+" "+ans+" "+user, response, recipients); }));
      }

      return reply_to_client(filedes, response);
    }
}

int main (void)
{
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  socklen_t size;

  /* Create the socket and set it up to accept connections. */
  sock = make_socket (PORT);

  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      exit (EXIT_FAILURE);
    }

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  cerr << "Server Ready\n";
  
  while (1)
    {
      /* Clean up futures at a certain threshold. */
      if (futures.size() > 100)
        {
          futures.erase(futures.begin(), futures.begin() + (futures.size() * 4/5)); 
        }

      /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
          perror ("select");
          exit (EXIT_FAILURE);
        }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == sock)
              {
                /* Connection request on original socket. */
                int new_fd;
                size = sizeof (clientname);
                new_fd = accept (sock,
                              (struct sockaddr *) &clientname,
                              &size);
                if (new_fd < 0)
                  {
                    perror ("accept");
                    exit (EXIT_FAILURE);
                  }
                fprintf (stderr,
                         "Server: connect from host %s, port %hu, fd %d.\n",
                         inet_ntoa (clientname.sin_addr),
                         ntohs (clientname.sin_port),
                         new_fd);
                FD_SET (new_fd, &active_fd_set);
              }
            else
              {
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i) < 0)
                  {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }
    }
}
