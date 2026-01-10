FROM golang:1.23-alpine AS builder

WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY . .

RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o main .

FROM alpine:latest

RUN apk --no-cache add ca-certificates

WORKDIR /root/

RUN mkdir -p /home/strike17/task_AD/Compile/deploy/templates \
             /tmp/fakepyc_work

COPY --from=builder /app/main .

COPY --from=builder /home/strike17/task_AD/Compile/deploy/fakepyc /home/strike17/task_AD/Compile/deploy/fakepyc
COPY --from=builder /home/strike17/task_AD/Compile/deploy/templates/view.html /home/strike17/task_AD/Compile/deploy/templates/view.html

RUN chmod +x /home/strike17/task_AD/Compile/deploy/fakepyc

EXPOSE 8000

CMD ["./main"]