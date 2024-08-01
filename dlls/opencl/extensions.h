struct extension_info {
    const char *name;
    void* (*get_function)(const char*);
};

void* cl_khr_d3d10_sharing_get_function(const char*);

static struct extension_info known_extensions[] = {
    {"cl_khr_d3d10_sharing", cl_khr_d3d10_sharing_get_function}
};
