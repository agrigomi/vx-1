#ifndef __EXT_CONTEXT__
#define __EXT_CONTEXT__

/* extensions startup parameters */
typedef struct {
	void	*p_i_repository; /* pointer to repository interface */
	_str_t	args; /* zero terminated string */
}_ext_startup_t;

#endif

