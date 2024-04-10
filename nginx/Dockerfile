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
RUN ln -s /code/nginx/nginx.conf /etc/nginx/conf.d/default.conf

# Append the `daemon off` directive to the config,
# so `nginx` always runs in the foreground
# Instead you can also comment this out and use `nginx -g 'daemon off;'
RUN echo 'daemon off;' >> /etc/nginx/nginx.conf

WORKDIR /code
