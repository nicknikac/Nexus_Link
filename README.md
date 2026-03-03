### Nexus Link: Distributed C++ Engine

Nexus Link is a small web server written in C++ using raw TCP sockets. I built it to understand what’s really happening when a backend receives an HTTP request and sends a response, without relying on a web framework.

Even though the app logic is simple, it runs in a more “real” infrastructure setup. Nginx sits in front as a reverse proxy, Redis stores shared state for the visitor counter, and Docker Compose ties everything together so you can run the whole system with one command.

### Why I built this

The main challenge I wanted to explore was shared state across multiple instances. If you run two app containers, each one has its own memory, so a normal in-process counter won’t match between them. In this project the counter lives in Redis, so any instance can increment the same key and the visitor count stays global.

### Stack

The core server is C++ with manual socket handling and a hand-built HTTP response. Redis is used as a shared state layer for the counter (`visitor_count`). Nginx listens on port 80 and forwards traffic to the app service on port 5000. Docker and Docker Compose build the C++ binary, run the services, and provide networking plus a persistent Redis volume.

### What it does

When you visit `/`, the server increments the visitor counter in Redis and returns an HTML dashboard. When you visit `/reset`, it sets the counter back to 0 and redirects back to `/`.

On startup, the C++ process retries the Redis connection until Redis is available. This avoids failing during container orchestration when services start at slightly different times.

Redis data is stored in a named Docker volume, so the visitor count survives container restarts. If you remove volumes (for example with `docker compose down -v`) the stored counter will be wiped.

<img width="965" height="728" alt="NexusScreenshot" src="https://github.com/user-attachments/assets/a7de0640-4a23-4e83-bd3d-91fae5e0b8bc" />

### How it works

```mermaid
flowchart LR
  B[Browser] --> N[Nginx :80]
  N --> A[App container(s) :5000<br/>C++ server]
  A <--> R[Redis<br/>visitor_count key]
  A --> H[HTML dashboard<br/>(/ and /reset)]
  H --> B
```


### How to run

You’ll need Docker and Docker Compose installed.

Clone the repo:

```bash
git clone <your-repo-link>
cd prelude_c++_proj
```

Start the system:

```bash
docker compose up --build
```

Open the dashboard:

```text
http://localhost
```

If you want to run more than one app instance:

```bash
docker compose up --build --scale app=2
```

Nginx is configured to distribute traffic across the replicas (connection-based). The easiest way to see two different nodes is to open the site in a second browser or an incognito window and compare the “Active Node” value on the page. The visitor count stays consistent because it’s stored in Redis.

### Files to look at

`main.cpp` contains the socket server, basic request handling, and Redis counter logic. `nginx.conf` contains the reverse proxy and upstream configuration. `docker-compose.yml` defines the services and the Redis volume. `Dockerfile` builds the C++ binary and links `hiredis`.
