typedef struct channel channel;

int disconnect_each_user(channel ch)
{
	int i = 0;
	for(i < ch.user_count; ++i)
		ch.users[i]->disconnect();
	return 0;
}
