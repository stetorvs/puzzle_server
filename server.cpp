#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

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

// Group email data
map<string, string> group_2_email = {
  {"The Hosts", "stetorvs@gmail.com"},
};

void verify_emails()
{
  string emails;
  for (auto& [team, email] : group_2_email) {
    cout << "Verifying team: " << team << '\n';
    assert(group_2_members.count(team) != 0U);
    emails += email;
    emails += ' ';
  }
}

int make_socket(uint16_t port)
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

int reply_to_client(int filedes, string msg)
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

string sanitize(string st)
{
  size_t pos = st.find('\'');
  while (pos != string::npos) {
    st.replace(pos, 1, "'\\''");
    pos = st.find('\'', pos+4);
  }
  return st;
}

void email(string msg, string subject, string to)
{
  if (msg.back() == '\n') {
    msg.pop_back();
  }
  if (subject.back() == '\n') {
    subject.pop_back();
  }
  string cmd = "echo '" + sanitize(msg) + "' | mail -s 'Puzzle: " + sanitize(subject) + "' '" + sanitize(to) + "'";
  system(cmd.c_str());
}

void update_scoreboard(string group, string question)
{
  lock_guard<mutex> write_lock(write_mutex);

  vector<string> questions = {"cheating", "crash", "debug_msg", "difference"};

  // Laziness from static...
  static map<string, map<string, int>> score_standings;
  if (score_standings.empty()) {
    map<string, int> empty;
    transform(questions.begin(), questions.end(), inserter(empty, empty.end()), [] (string& name) { return pair(name, 0); });
    for (auto& p : group_2_members) {
      score_standings[p.first] = empty;
    }
  }

  static map<string, string> last_solve;
  if (last_solve.empty()) {
    for (auto& p : group_2_members) {
      time_t ls_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
      string t(ctime(&ls_time));
      last_solve[p.first] = t.substr(0, t.length()-1); // Strip newline
    }
  }

  // Now actually update
  if (score_standings[group][question] != 0) {
    return;
  }

  ofstream ofs("standings.txt");

  if (question == "meta") {
    score_standings[group][question] = 3;
  } else {
    score_standings[group][question] = 1;
  }
  time_t ls_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
  string t(ctime(&ls_time));
  last_solve[group] = t.substr(0, t.length()-1); // Strip newline

  // File update
  for (auto& p : group_2_members) {
    const string& team = p.first;
    ofs << "| " << team << " | ";
    ofs << accumulate(score_standings[team].begin(), score_standings[team].end(), 0, [] (int sum, const pair<string, int>& p) { return sum + p.second; });
    ofs << " | " << last_solve[team] << " |";
    for (auto& score_pair : score_standings[team]) {
      ofs << ' ' << score_pair.second << " |";
    }
    ofs << '\n';
  }
}

int read_from_client (int filedes, const string& ip)
{
  char buffer[MAXMSG];
  int nbytes;

  // static because I'm lazy
  static auto member_2_group_map = get_member_2_group(group_2_members);
  static map<pair<string, string>, chrono::time_point<chrono::system_clock>> ip_group_2_time;

  nbytes = read (filedes, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      return -2;
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

      for (char& c : question) c = tolower(c);
      for (char& c : ans) c = tolower(c);

      auto find_it = member_2_group_map.find(user);
      string response = string("Sorry, user ") + user + string(" does not belong to any team\n");
      if (find_it != end(member_2_group_map)) {
        // Check timestamp
        chrono::time_point<chrono::system_clock> curr_t = chrono::system_clock::now();
        auto& last_t = ip_group_2_time[pair(ip, find_it->second)];
        chrono::duration<double> dur_t = curr_t - last_t;
        constexpr double timeout = 15.;
        if (dur_t.count() < timeout) {
          response = string("Please wait ") + to_string(timeout - dur_t.count()) + " seconds before submitting\n";
          return reply_to_client(filedes, response);
        }
        last_t = curr_t;
        bool correct = false;
        auto group = find_it->second;
        response = get_response(question, ans, correct);
        response += "Team name: " + group + '\n';
        string recipients = DEFAULT_EMAIL + " " + group_2_email[group];
        if (correct) {
          futures.emplace_back(async(launch::async, [=] () { update_scoreboard(group, question); }));
        }
        futures.emplace_back(async(launch::async, [=] () { email(question+" "+ans+" "+user, response, recipients); }));
      } else if (user == "guest") {
        // Check timestamp
        string guest_email;
        ss >> guest_email;
        chrono::time_point<chrono::system_clock> curr_t = chrono::system_clock::now();
        auto& last_t = ip_group_2_time[pair(ip, guest_email)];
        chrono::duration<double> dur_t = curr_t - last_t;
        constexpr double timeout = 15.;
        if (dur_t.count() < timeout) {
          response = string("Please wait ") + to_string(timeout - dur_t.count()) + " seconds before submitting\n";
          return reply_to_client(filedes, response);
        }
        last_t = curr_t;
        bool correct = false;
        auto group = find_it->second;
        response = get_response(question, ans, correct);
        response += "Guest team\n";
        string recipients = DEFAULT_EMAIL + " " + guest_email;
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
  map<int, string> fd_to_ip;

  verify_emails();

  /* Ignore SIGPIPE errors. */
  signal(SIGPIPE, SIG_IGN);

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
                fd_to_ip[new_fd] = inet_ntoa (clientname.sin_addr);
              }
            else
              {
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i, fd_to_ip[i]) < 0)
                  {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }
    }
}
