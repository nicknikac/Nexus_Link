FROM gcc:latest

# Redis client library for C++
RUN apt-get update && apt-get install -y libhiredis-dev

WORKDIR /app
COPY main.cpp .

# Build the server binary
RUN g++ -o web_engine main.cpp -lhiredis

CMD ["./web_engine"]