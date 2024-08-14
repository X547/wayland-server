using WlProxyConstructor = WlProxy*();


class WlProxy {
private:
	struct wl_proxy *fProxy = NULL;

public:
	static WlProxy *FromProxy(struct wl_proxy *proxy);
	struct wl_proxy *ToProxy() const {return fProxy;}

	virtual int Dispatch(uint32_t opcode, const struct wl_message *message, union wl_argument *args);
};


class WlClient: public WlProxy {
public:
	int Dispatch(uint32_t opcode, const struct wl_message *message, union wl_argument *args) override;

	void SendSync(WlCallback *callback);
	void SendGetRegistry(WlRegistry *registry);

	virtual void HandleError(WlProxy *object, uint32_t code, const char *message) = 0;
	virtual void HandleDeleteId(uint32_t id);
};


int WlClient::Dispatch(uint32_t opcode, const struct wl_message *message, union wl_argument *args)
{
	switch (opcode) {
		case 0:
			HandleError(args[0].o, args[1].u, args[2].s);
			return 0;
		case 1:
			HandleDeleteId(args[0].u);
			return 0;
	}
	return -1;
}

void WlClient::SendSync(WlCallback *callback)
{
	callback->Init(wl_proxy_marshal_flags(ToProxy(), 0, &wl_callback_interface, Version(), 0, NULL);
}

void WlClient::SendGetRegistry(WlRegistry *registry)
{
	registry->Init(wl_proxy_marshal_flags(ToProxy(), 1, &wl_callback_interface, Version(), 0, NULL);
}
