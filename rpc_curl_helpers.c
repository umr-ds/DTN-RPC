#include "rpc.h"

// Function to store results of curl calls (if not free'd manually, all responses are stored consecutively).
size_t _rpc_curl_write_response (void *contents, size_t size, size_t nmemb, void *userp) {
    // realsize is size times number of bytes (nmemb)
    size_t realsize = size * nmemb;
    struct CurlResultMemory *mem = (struct CurlResultMemory *) userp;

    // realloc enough memory for the result.
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        pwarn("Not enough memory for cURL response!");
        return 0;
    }

    // Copy the response to the reallocated memory.
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    // Set the size to the new size.
    mem->size += realsize;
    // Reset the old one.
    mem->memory[mem->size] = 0;
    return realsize;
}

// Function to store results of curl calls (if not free'd manually, all responses are stored consecutively).
size_t _rpc_curl_write_to_file (void *contents, size_t size, size_t nmemb, FILE *dst_file) {
    size_t written = fwrite(contents, size, nmemb, dst_file);
    return written;
}

// Init memory where cURL results are written.
void _rpc_curl_init_memory (struct CurlResultMemory *curl_result_memory) {
    curl_result_memory->memory = calloc(1, 1);
    curl_result_memory->size = 0;
}

// Reinit memory where cURL results are written.
void _rpc_curl_reinit_memory (struct CurlResultMemory *curl_result_memory) {
    free(curl_result_memory->memory);
    curl_result_memory->memory = calloc(1, 1);
    curl_result_memory->size = 0;
}

// Free memory where cURL results were written.
void _rpc_curl_free_memory (struct CurlResultMemory *curl_result_memory) {
    free(curl_result_memory->memory);
    curl_result_memory->size = 0;
}

// Set basic cURL options (URL, HTTP basic authentication, credentials and header (even if it's empty at this point.))
void _rpc_curl_set_basic_opt (char* url, CURL *curl_handler, struct curl_slist *header) {
    curl_easy_setopt(curl_handler, CURLOPT_URL, url);
    curl_easy_setopt(curl_handler, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl_handler, CURLOPT_USERNAME, "RPC");
    curl_easy_setopt(curl_handler, CURLOPT_PASSWORD, "SRPC");
    curl_easy_setopt(curl_handler, CURLOPT_HTTPHEADER, header);
}

// Add the manifest and payload form and add the form to the cURL request.
void _rpc_curl_add_file_form (const char *tmp_manifest_file_name, const char *tmp_payload_file_name, CURL *curl_handler, struct curl_httppost *formpost, struct curl_httppost *lastptr) {
    // Add the manifest form.
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "manifest", CURLFORM_FILE, tmp_manifest_file_name, CURLFORM_CONTENTTYPE, "rhizome/manifest; format=text+binarysig", CURLFORM_END);

    // Add the payload form.
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "payload", CURLFORM_FILE, tmp_payload_file_name, CURLFORM_CONTENTTYPE, "application/octet-stream", CURLFORM_END);

    // Add the forms to the request.
    curl_easy_setopt(curl_handler, CURLOPT_HTTPPOST, formpost);
}
