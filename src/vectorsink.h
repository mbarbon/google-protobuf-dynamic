#ifndef _GPD_XS_SINK_INCLUDED
#define _GPD_XS_SINK_INCLUDED

#include <upb/sink.h>

namespace gpd {

// this is pretty much the same as upb_bufsink in uPB, but it does not
// rely on an environment for memory allocation (so the buffer is
// retained)
class VectorSink {
public:
    VectorSink() {
        SetHandler(&handler_);
        input_.Reset(&handler_, this);

        length = 0;
        capacity = 100;
        buffer = (char *) malloc(capacity);
    }

    upb::BytesSink* input() { return &input_; }

    const char *data() {
        return buffer;
    }

    size_t size() {
        return length;
    }

 private:
    static void SetHandler(upb::BytesHandler* handler) {
        upb_byteshandler_setstartstr(handler, &VectorSink::StartString, NULL);
        upb_byteshandler_setstring(handler, &VectorSink::StringBuf, NULL);
    }

    static void* StartString(void *c, const void *hd, size_t size) {
        VectorSink *sink = static_cast<VectorSink*>(c);
        sink->length = 0;
        return c;
    }

    static size_t StringBuf(void* c, const void* hd, const char* buf, size_t n,
                          const upb::BufferHandle* h) {
        VectorSink *sink = static_cast<VectorSink*>(c);

        while (sink->length + n > sink->capacity) {
            sink->capacity *= 2;
            sink->buffer = (char *) realloc(sink->buffer, sink->capacity);
        }

        memcpy(sink->buffer + sink->length, buf, n);
        sink->length += n;

        return n;
    }

    upb::BytesHandler handler_;
    upb::BytesSink input_;

    char *buffer;
    size_t length, capacity;
};

}

#endif
