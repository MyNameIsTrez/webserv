# docker build -t nginx .
# docker run --rm -it -v $(pwd):/code -p 8080:80 nginx
# nginx
# View http://localhost:8080/

FROM alpine:3.13

RUN apk update && \
	apk add \
		curl \
		xz \
		tar \
		python3 \
		nginx

RUN mkdir -p /run/nginx

RUN rm /etc/nginx/conf.d/default.conf
RUN ln -s /code/nginx.conf /etc/nginx/conf.d/default.conf

RUN echo 'daemon off;' >> /etc/nginx/nginx.conf

EXPOSE 80

WORKDIR /code
