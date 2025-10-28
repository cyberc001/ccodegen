// Тест multidecl1:
// Глобальные объявления разных видов.

struct vec {
	double x, y;
	double z;
} vec_one = {1, 0};
struct neg_log_likelihood_ns_loss_args {
	struct vec pre_y;
};

double squared_loss(struct vec y, struct vec _y, void* args)
{
	double l = 0;
	for(size_t i = 0; i < y.n; ++i)
		l += pow(y.data[i] - _y.data[i], 2);
	return l / y.n;
}

enum {
	GRAD_OK = 0,
	GRAD_STOPPED_THRES, GRAD_STOPPED_ITERS,
	GRAD_INF = -10
};

double neg_log_likelihood_ns_loss(struct vec _y, struct vec y, void* _args)
{
	struct neg_log_likelihood_ns_loss_args* args = _args;
	double l = -log(_y.data[args->neg_idx[0]]);
	for(size_t i = 1; i < args->neg_ln + 1; ++i)
		l -= log(sigmoid(-args->pre_y.data[args->neg_idx[i]]));
	return l;
}

enum shader_result {
	SHADER_COMPILE_SYNTAX_ERROR,
	SHADER_COMPILE_LINK_ERROR = 3,
	SHADER_RUNTIME_ERROR = -3,
	SHADER_COMPILING,
	SHADER_OK = 0
};
