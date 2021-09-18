
#ifndef HEADER_JSON_PARSER_DFINITION
#define HEADER_JSON_PARSER_DFINITION 1

#include <new>
#include <limits>
#include <vector>
#include <cuchar>
#include <climits>
#include <utility>
#include <algorithm>
#include <stdexcept>

namespace json {

    enum boolean : bool { False = false, True = true };

    namespace details {

        template<typename char_t>
        inline static bool isWhitespace(char_t c) noexcept { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; }

        template<typename char_t>
        inline static bool isDigit(char_t c) noexcept { return c >= '0' && c <= '9'; }

        template<typename istream_t>
        inline static auto streamPeek(istream_t & s) -> decltype(s.peek()) {
            auto v = s.peek();
            if (v == istream_t::traits_type::eof()) { throw std::runtime_error("failed to read json"); }
            return v;
        }

        template<typename istream_t, typename int_t>
        inline static auto streamGet(istream_t & s, int_t & charsRead) -> decltype(s.get()) {
            auto v = s.get();
            if (v == istream_t::traits_type::eof()) { throw std::runtime_error("failed to read json"); }
            charsRead += 1;
            return v;
        }

        template<typename istream_t, typename literal_t, typename int_t>
        inline static void readLiteral(istream_t & s, literal_t & r, int_t & charsRead) {
            if (streamGet(s, charsRead) != '"') { throw std::runtime_error("failed to parse json"); }
            bool esc = false;
            for (auto c = streamPeek(s);; c = streamPeek(s)) {
                streamGet(s, charsRead);
                if (esc) {
                    switch (c) {
                        case '"': r.append("\""); break;
                        case '\\': r.append("\\"); break;
                        case '/': r.append("/"); break;
                        case 'b': r.append("\b"); break;
                        case 'f': r.append("\f"); break;
                        case 'n': r.append("\n"); break;
                        case 'r': r.append("\r"); break;
                        case 't': r.append("\t"); break;
                        case 'u':
                        {
                            union {
                                char b[4];
                                char32_t e;
                            };
                            static_assert(sizeof(char32_t) == (sizeof(char) * 4), "unexpected char32_t size");
                            for (int i = 0; i < 4; ++i) {
                                auto d = b[i] = streamGet(s, charsRead);
                                if (!((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') || (d >= 'A' && d <= 'F'))) {
                                    throw std::runtime_error("failed to parse json: illegal escape character");
                                }
                            }
                            std::mbstate_t state{};
                            char buf[MB_LEN_MAX]{};
                            if (std::c32rtomb(buf, e, &state) == -1) { throw std::runtime_error("failed to parse json: illegal u32 character"); }
                            r.append(buf);
                            break;
                        }
                        default: throw std::runtime_error("failed to parse json: illegal escape character");
                    }
                    esc = false;
                    continue;
                }
                if (c == '\\') { esc = true; continue; }
                if (c == '"') { break; }
                {
                    char b[2];
                    b[0] = c;
                    b[1] = 0;
                    r.append(b);
                }
            }
        }

        template<typename ostream_t, typename literal_t>
        inline static void writeLiteral(ostream_t & s, const literal_t & l) {
            s << '\"';
            for (auto c : l) {
                switch (c) {
                    case '"': s << "\\\""; break;
                    case '\\': s << "\\\\"; break;
                    case '/': s << "\\/"; break;
                    case '\b': s << "\\b"; break;
                    case '\f': s << "\\f"; break;
                    case '\n': s << "\\n"; break;
                    case '\r': s << "\\r"; break;
                    case '\t': s << "\\t"; break;
                    default: s << c;
                }
            }
            s << '\"';
        }

        template<typename x, bool numericLimitsSpecialized>
        struct compare_primitive {
            static bool equal(const x & a, const x & b) noexcept(noexcept(a == b)) { return a == b; }
        };
        template<typename x>
        struct compare_primitive<x, true> {
            static bool equal(const x & a, const x & b) { return (a > b ? (a - b) : (b - a)) <= std::numeric_limits<x>::epsilon(); }
        };

        enum struct primitive_type { null, number, literal, boolean };

        template<typename number_t, typename literal_t, typename boolean_t>
        struct primitive_base {
        private:

            struct vfptr_t {
                virtual ~vfptr_t() = default;
                virtual primitive_type type() const noexcept { return primitive_type::null; }
                virtual bool null() const noexcept { return true; }
                virtual number_t & number() { throw std::bad_cast(); }
                virtual const number_t & number() const { throw std::bad_cast(); }
                virtual literal_t & literal() { throw std::bad_cast(); }
                virtual const literal_t & literal() const { throw std::bad_cast(); }
                virtual boolean_t & boolean() { throw std::bad_cast(); }
                virtual const boolean_t & boolean() const { throw std::bad_cast(); }
                virtual bool equal(const vfptr_t & p) const { return p.type() == primitive_type::null; }
                virtual void copyTo(vfptr_t * t) const { this->reinit_base<vfptr_t>(t); }
                virtual void moveTo(vfptr_t * t) { this->reinit_base<vfptr_t>(t); }

                template<typename target_t>
                target_t * reinit_base(vfptr_t * t) const {
                    if (t->type() != type()) {
                        t->~vfptr_t();
                        return new (t) target_t();
                    }
                    return static_cast<target_t *>(t);
                }

            };

            struct vfptr_non_null_t : vfptr_t { virtual bool null() const noexcept override { return false; } };

            struct vfptr_number_t : vfptr_non_null_t {
                number_t value;
                virtual primitive_type type() const noexcept override { return primitive_type::number; }
                virtual number_t & number() override { return value; }
                virtual const number_t & number() const override { return value; }
                virtual bool equal(const vfptr_t & p) const {
                    return p.type() == primitive_type::number &&
                        compare_primitive<number_t, std::numeric_limits<number_t>::is_specialized>::equal(static_cast<const vfptr_number_t &>(p).value, value);
                }
                virtual void copyTo(vfptr_t * t) const { this->template reinit_base<vfptr_number_t>(t)->value = value; }
                virtual void moveTo(vfptr_t * t) { this->template reinit_base<vfptr_number_t>(t)->value = std::move(value); }
            };

            struct vfptr_literal_t : vfptr_non_null_t {
                literal_t value;
                virtual primitive_type type() const noexcept override { return primitive_type::literal; }
                virtual literal_t & literal() override { return value; }
                virtual const literal_t & literal() const override { return value; }
                virtual bool equal(const vfptr_t & p) const { return p.type() == primitive_type::literal && static_cast<const vfptr_literal_t &>(p).value == value; }
                virtual void copyTo(vfptr_t * t) const { this->template reinit_base<vfptr_literal_t>(t)->value = value; }
                virtual void moveTo(vfptr_t * t) { this->template reinit_base<vfptr_literal_t>(t)->value = std::move(value); }
            };

            struct vfptr_boolean_t : vfptr_non_null_t {
                boolean_t value;
                virtual primitive_type type() const noexcept override { return primitive_type::boolean; }
                virtual boolean_t & boolean() override { return value; }
                virtual const boolean_t & boolean() const override { return value; }
                virtual bool equal(const vfptr_t & p) const { return p.type() == primitive_type::boolean && static_cast<const vfptr_boolean_t &>(p).value == value; }
                virtual void copyTo(vfptr_t * t) const { this->template reinit_base<vfptr_boolean_t>(t)->value = value; }
                virtual void moveTo(vfptr_t * t) { this->template reinit_base<vfptr_boolean_t>(t)->value = std::move(value); }
            };

            union vfptr_storage_t {
                vfptr_t m_null;
                vfptr_number_t m_number;
                vfptr_literal_t m_literal;
                vfptr_boolean_t m_boolean;
                vfptr_storage_t() {}
                ~vfptr_storage_t() {}
            } m_vfptr;

        public:

            primitive_base() { new (&m_vfptr) vfptr_t(); }
            primitive_base(decltype(nullptr)) : primitive_base() {}
            primitive_base(number_t literal) { (new (&m_vfptr) vfptr_number_t())->value = std::move(literal); }
            primitive_base(literal_t literal) { (new (&m_vfptr) vfptr_literal_t())->value = std::move(literal); }
            primitive_base(boolean_t literal) { (new (&m_vfptr) vfptr_boolean_t())->value = std::move(literal); }

            primitive_base(const primitive_base & c) : primitive_base() { *this = c; }
            primitive_base(primitive_base && m) : primitive_base() { *this = std::move(m); }
            ~primitive_base() { reinterpret_cast<const vfptr_t *>(&m_vfptr)->~vfptr_t(); }

            primitive_base & operator=(const primitive_base & c) {
                reinterpret_cast<const vfptr_t &>(c.m_vfptr).copyTo(reinterpret_cast<vfptr_t &>(m_vfptr));
                return *this;
            }

            primitive_base & operator=(primitive_base && m) {
                reinterpret_cast<vfptr_t &>(m.m_vfptr).moveTo(reinterpret_cast<vfptr_t &>(m_vfptr));
                return *this;
            }

            primitive_type type() const noexcept { return reinterpret_cast<const vfptr_t &>(m_vfptr).type(); }
            bool isNull() const { return reinterpret_cast<const vfptr_t &>(m_vfptr).null(); }
            number_t & number() { return reinterpret_cast<vfptr_t &>(m_vfptr).number(); }
            const number_t & number() const { return reinterpret_cast<const vfptr_t &>(m_vfptr).number(); }
            literal_t & literal() { return reinterpret_cast<vfptr_t &>(m_vfptr).literal(); }
            const literal_t & literal() const { return reinterpret_cast<const vfptr_t &>(m_vfptr).literal(); }
            boolean_t & boolean() { return reinterpret_cast<vfptr_t &>(m_vfptr).boolean(); }
            const boolean_t & boolean() const { return reinterpret_cast<const vfptr_t &>(m_vfptr).boolean(); }
            bool operator==(const primitive_base & p) const { return reinterpret_cast<const vfptr_t &>(m_vfptr).equal(reinterpret_cast<const vfptr_t &>(p.m_vfptr)); }

        };

        template<
            template<typename, typename...> typename uptr_t_,
            template<typename, typename, typename...> typename object_t_,
            template<typename, typename...> typename array_t_,
            typename number_t_,
            typename literal_t_,
            typename ostream_t_
        >
        struct definition {

            struct var_t;
            struct object_t;
            struct array_t;
            struct primitive_t;

            using number_t = number_t_;
            using literal_t = literal_t_;

            struct var_t {
                using ptr_t = uptr_t_<var_t>;
                virtual ~var_t() = default;

            protected:
                var_t() = default;

                static void writeIndent0(ostream_t_ & s, int indent) {
                    for (int i = 0; i < indent; ++i) { s << ' '; }
                }

            public:
                virtual bool isObject() const noexcept { return false; }
                virtual object_t * tryAsObject() noexcept { return nullptr; }
                virtual const object_t * tryAsObject() const noexcept { return nullptr; }
                virtual object_t & asObject() { throw std::bad_cast(); }
                virtual const object_t & asObject() const { throw std::bad_cast(); }

                virtual bool isArray() const noexcept { return false; }
                virtual array_t * tryAsArray() noexcept { return nullptr; }
                virtual const array_t * tryAsArray() const noexcept { return nullptr; }
                virtual array_t & asArray() { throw std::bad_cast(); }
                virtual const array_t & asArray() const { throw std::bad_cast(); }

                virtual bool isPrimitive() const noexcept { return false; }
                virtual primitive_t * tryAsPrimitive() noexcept { return nullptr; }
                virtual const primitive_t * tryAsPrimitive() const noexcept { return nullptr; }
                virtual primitive_t & asPrimitive() { throw std::bad_cast(); }
                virtual const primitive_t & asPrimitive() const { throw std::bad_cast(); }

                virtual void print(ostream_t_ & s, int indent = -1, int layer = 0) const = 0;

                virtual bool operator==(const var_t &) const { return false; }
                virtual bool operator==(decltype(nullptr)) const { return false; }
                virtual bool operator==(json::boolean) const { return false; }
                virtual bool operator==(number_t) const { return false; }
                virtual bool operator==(const literal_t &) const { return false; }

                bool operator!=(const var_t & x) const { return !operator==(x); }
                bool operator!=(decltype(nullptr) x) const { return !operator==(x); }
                bool operator!=(json::boolean x) const { return !operator==(x); }
                bool operator!=(number_t x) const { return !operator==(x); }
                bool operator!=(const literal_t & x) const { return !operator==(x); }

                friend bool operator==(const ptr_t & x, decltype(nullptr)) { return !x || *x == nullptr; }
                friend bool operator!=(const ptr_t & x, decltype(nullptr)) { return x && *x != nullptr; }

                friend bool operator==(const ptr_t & a, const ptr_t & b) { return a == nullptr ? b == nullptr : (b != nullptr && (*a == *b)); }
                friend bool operator!=(const ptr_t & a, const ptr_t & b) { return a == nullptr ? b != nullptr : (b == nullptr || (*a != *b)); }

            };

            struct object_t : var_t, object_t_<literal_t, uptr_t_<var_t>> {
                using ptr_t = uptr_t_<object_t>;
                using base_t = object_t_<literal_t, uptr_t_<var_t>>;

                template<typename ... args_t>
                explicit object_t(args_t && ... args) : base_t(std::forward<args_t>(args)...) {}

                virtual bool isObject() const noexcept override { return true; }
                virtual object_t * tryAsObject() noexcept override { return this; }
                virtual const object_t * tryAsObject() const noexcept override { return this; }
                virtual object_t & asObject() noexcept override { return *this; }
                virtual const object_t & asObject() const noexcept override { return *this; }

                virtual void print(ostream_t_ & s, int indent = -1, int layer = 0) const override {
                    s << '{';
                    if (!this->empty()) {
                        if (indent >= 0) {
                            s << "\n";
                            this->writeIndent0(s, indent * (layer + 1));
                        }
                        for (auto i = this->begin(), l = this->end();;) {
                            writeLiteral(s, i->first);
                            s << (indent >= 0 ? ": " : ":");
                            if (i->second) {
                                i->second->print(s, indent, layer + 1);
                            } else {
                                primitive_t().print(s, indent, layer + 1);
                            }
                            ++i;
                            if (i != l) { s << ","; } else { break; }
                            if (indent >= 0) {
                                s << "\n";
                                this->writeIndent0(s, indent * (layer + 1));
                            }
                        }
                        if (indent >= 0) {
                            s << "\n";
                            this->writeIndent0(s, indent * layer);
                        }
                    }
                    s << '}';
                }

                template<typename literal_t__>
                object_t & set(literal_t__ key, typename var_t::ptr_t value) {
                    this->operator[](std::move(key)) = std::move(value);
                    return *this;
                }

                virtual bool operator==(const var_t & v) const override {
                    auto o = v.tryAsObject();
                    return o && (*static_cast<const base_t *>(this) == *static_cast<const base_t *>(o));
                }

            };

            struct array_t : var_t, array_t_<uptr_t_<var_t>> {
                using ptr_t = uptr_t_<array_t>;
                using base_t = array_t_<uptr_t_<var_t>>;

                template<typename ... args_t>
                explicit array_t(args_t && ... args) : base_t(std::forward<args_t>(args)...) {}

                virtual bool isArray() const noexcept override { return true; }
                virtual array_t * tryAsArray() noexcept override { return this; }
                virtual const array_t * tryAsArray() const noexcept override { return this; }
                virtual array_t & asArray() noexcept override { return *this; }
                virtual const array_t & asArray() const noexcept override { return *this; }

                virtual void print(ostream_t_ & s, int indent = -1, int layer = 0) const override {
                    s << '[';
                    if (!this->empty()) {
                        if (indent >= 0) {
                            s << "\n";
                            this->writeIndent0(s, indent * (layer + 1));
                        }
                        for (auto i = this->begin(), l = this->end();;) {
                            if (*i) {
                                (**i).print(s, indent, layer + 1);
                            } else {
                                primitive_t().print(s, indent, layer + 1);
                            }
                            ++i;
                            if (i != l) { s << ","; } else { break; }
                            if (indent >= 0) {
                                s << "\n";
                                this->writeIndent0(s, indent * (layer + 1));
                            }
                        }
                        if (indent >= 0) {
                            s << "\n";
                            this->writeIndent0(s, indent * layer);
                        }
                    }
                    s << ']';
                }

                array_t & add(typename var_t::ptr_t value) {
                    this->emplace_back(std::move(value));
                    return *this;
                }

                virtual bool operator==(const var_t & v) const override {
                    auto o = v.tryAsArray();
                    return o && (*static_cast<const base_t *>(this) == *static_cast<const base_t *>(o));
                }

            };

            struct primitive_t : var_t, primitive_base<number_t, literal_t, json::boolean> {
                using ptr_t = uptr_t_<primitive_t>;
                using base_t = primitive_base<number_t, literal_t, json::boolean>;
                using type_t = primitive_type;

                using base_t::base_t;

                virtual bool isPrimitive() const noexcept override { return true; }
                virtual primitive_t * tryAsPrimitive() noexcept override { return this; }
                virtual const primitive_t * tryAsPrimitive() const noexcept override { return this; }
                virtual primitive_t & asPrimitive() noexcept override { return *this; }
                virtual const primitive_t & asPrimitive() const noexcept override { return *this; }

                virtual void print(ostream_t_ & s, int indent = -1, int layer = 0) const override {
                    switch (this->type()) {
                        case type_t::number: s << this->number(); break;
                        case type_t::literal: writeLiteral(s, this->literal()); break;
                        case type_t::boolean: s << (static_cast<bool>(this->boolean()) ? "true" : "false"); break;
                        default: s << "null"; break;
                    }
                }

                virtual bool operator==(const var_t & v) const override {
                    auto p = v.tryAsPrimitive();
                    return p && static_cast<const base_t &>(*this) == static_cast<const base_t &>(*p);
                }

                virtual bool operator==(decltype(nullptr)) const noexcept override { return this->isNull(); }
                virtual bool operator==(json::boolean b) const { return this->type() == type_t::boolean && b == this->boolean(); }
                virtual bool operator==(number_t n) const {
                    return this->type() == type_t::number && compare_primitive<number_t, std::numeric_limits<number_t>::is_specialized>::equal(n, this->number());
                }
                virtual bool operator==(const literal_t & l) const { return this->type() == type_t::literal && l == this->literal(); }

            };

        };

        template<typename definition_t_, typename istream_t_>
        struct parser {

            using istream_t = istream_t_;
            using definition_t = definition_t_;

            using var_t = typename definition_t::var_t;
            using object_t = typename definition_t::object_t;
            using array_t = typename definition_t::array_t;
            using primitive_t = typename definition_t::primitive_t;
            using number_t = typename definition_t::number_t;
            using literal_t = typename definition_t::literal_t;

            std::vector<void(parser:: *)(void)> exe_stack;
            std::vector<typename var_t::ptr_t> var_stack;
            istream_t * s = nullptr;
            long long charsRead = 0;

            parser(istream_t * s) : s(s) {}
            ~parser() = default;

            auto streamPeek() -> decltype(details::streamPeek(*s)) { return details::streamPeek(*s); }

            auto streamGet() -> decltype(details::streamGet(*s, charsRead)) { return details::streamGet(*s, charsRead); }

            number_t readJsonNumber() {
                bool neg = false;
                if (streamPeek() == '-') {
                    streamGet();
                    neg = true;
                }
                if (!isDigit(streamPeek())) { throw std::runtime_error("failed to parse json"); }
                number_t n = 0, o = 0;
                while (true) {
                    auto c = streamPeek();
                    if (isDigit(c)) {
                        n = (n * 10) + (((number_t)c) - ((number_t)'0'));
                    } else if (c == '.') {
                        streamGet();
                        for (number_t m = 1;; m *= 10) {
                            c = streamPeek();
                            if (isDigit(c)) {
                                o += o * 10 + (((number_t)c) - ((number_t)'0'));
                            /*} else if (c == '.') {
                                throw std::runtime_error("failed to parse json");*/
                            } else {
                                n += o / m;
                                break;
                            }
                            streamGet();
                        }
                        break;
                    } else {
                        break;
                    }
                    streamGet();
                }
                return neg ? -n : n;
            }

            void skipWhitespaces() {
                for (auto c = streamPeek(); isWhitespace(c); c = streamPeek()) { streamGet(); }
            }

            void read() {
                skipWhitespaces();
                switch (streamPeek()) {

                    // object
                    case '{': exe_stack.emplace_back(&parser::objStart); break;

                    // array
                    case '[': exe_stack.emplace_back(&parser::arrStart); break;

                    // null
                    case 'n':
                    {
                        if (streamGet() != 'n' || streamGet() != 'u' || streamGet() != 'l' || streamGet() != 'l') {
                            throw std::runtime_error("failed to parse json");
                        }
                        var_stack.emplace_back(new primitive_t(nullptr));
                        break;
                    }

                    // true
                    case 't':
                    {
                        if (streamGet() != 't' || streamGet() != 'r' || streamGet() != 'u' || streamGet() != 'e') {
                            throw std::runtime_error("failed to parse json");
                        }
                        var_stack.emplace_back(new primitive_t(True));
                        break;
                    }

                    // true
                    case 'f':
                    {
                        if (streamGet() != 'f' || streamGet() != 'a' || streamGet() != 'l' || streamGet() != 's' || streamGet() != 'e') {
                            throw std::runtime_error("failed to parse json");
                        }
                        var_stack.emplace_back(new primitive_t(False));
                        break;
                    }

                    // literal
                    case '"':
                    {
                        literal_t l;
                        readLiteral(*s, l, charsRead);
                        var_stack.emplace_back(new primitive_t(std::move(l)));
                        break;
                    }

                    default:
                    {
                        auto n = readJsonNumber();
                        var_stack.emplace_back(new primitive_t(n));
                    }
                }
            }

            void objStart() {
                if (streamGet() != '{') { throw std::runtime_error("failed to parse json"); }
                var_stack.emplace_back(new object_t());
                exe_stack.emplace_back(&parser::objReadContent);
            }

            void objReadContent() {
                skipWhitespaces();
                if (streamPeek() == '"') { // kv pair
                    objReadKV();
                    return;
                }
                objEnd();
            }

            void objReadKV() {
                if (streamPeek() != '"') { throw std::runtime_error("failed to parse json"); }
                literal_t k;
                readLiteral(*s, k, charsRead);
                skipWhitespaces();
                if (streamGet() != ':') { throw std::runtime_error("failed to parse json"); }
                var_stack.emplace_back(new primitive_t(std::move(k)));
                exe_stack.emplace_back(&parser::objContinueKV);
                exe_stack.emplace_back(&parser::read);
            }

            void objContinueKV() {
                { // collect previously read kv
                    auto v = std::move(var_stack.back());
                    var_stack.pop_back();

                    {
                        auto k0 = dynamic_cast<primitive_t *>(var_stack.back().get());
                        if (k0 == nullptr || k0->type() != primitive_t::type_t::literal) { throw std::runtime_error("failed to parse json"); }
                    }
                    typename primitive_t::ptr_t k(static_cast<primitive_t *>(var_stack.back().release()));
                    var_stack.pop_back();

                    auto o = dynamic_cast<object_t *>(var_stack.back().get());
                    if (o == nullptr) { throw std::runtime_error("failed to parse json"); }
                    (*o)[std::move(k->literal())] = std::move(v);
                }
                skipWhitespaces();
                if (streamPeek() == ',') {
                    streamGet();
                    skipWhitespaces();
                    objReadKV();
                } else {
                    objEnd();
                }
            }

            void objEnd() {
                if (streamGet() != '}') { throw std::runtime_error("failed to parse json"); }
                if (dynamic_cast<object_t *>(var_stack.back().get()) == nullptr) { throw std::runtime_error("failed to parse json"); }
            }

            void arrStart() {
                if (streamGet() != '[') { throw std::runtime_error("failed to parse json"); }
                var_stack.emplace_back(new array_t());
                skipWhitespaces();
                if (streamPeek() != ']') {
                    exe_stack.emplace_back(&parser::arrReadElement);
                } else {
                    arrEnd();
                }
            }

            void arrReadElement() {
                exe_stack.emplace_back(&parser::arrContinueElements);
                exe_stack.emplace_back(&parser::read);
            }

            void arrContinueElements() {
                {
                    auto v = std::move(var_stack.back());
                    var_stack.pop_back();

                    auto a = dynamic_cast<array_t *>(var_stack.back().get());
                    if (a == nullptr) { throw std::runtime_error("failed to parse json"); }
                    a->emplace_back(std::move(v));
                }
                skipWhitespaces();
                if (streamPeek() == ',') {
                    streamGet();
                    skipWhitespaces();
                    arrReadElement();
                } else {
                    arrEnd();
                }
            }

            void arrEnd() {
                if (streamGet() != ']') { throw std::runtime_error("failed to parse json"); }
                if (dynamic_cast<array_t *>(var_stack.back().get()) == nullptr) { throw std::runtime_error("failed to parse json"); }
            }

            static typename var_t::ptr_t parse(istream_t & s) {
                struct reset_t {
                    istream_t & s;
                    typename istream_t::iostate e;

                    reset_t(istream_t & s) : s(s) {
                        e = s.exceptions();
                        s.exceptions(istream_t::badbit);
                    }

                    ~reset_t() {
                        s.exceptions(e);
                    }

                } s_reset{ s };

                parser p{ &s };

                p.skipWhitespaces();
                p.exe_stack.emplace_back(&parser::read);
                while (!p.exe_stack.empty()) {
                    auto f = p.exe_stack.back();
                    p.exe_stack.pop_back();
                    (p.*f)();
                }
                if (p.var_stack.size() != 1) { throw std::runtime_error("failed to parse json"); }

                return std::move(p.var_stack[0]);
            }

        };

        template<typename definition_t, typename istream_t>
        inline static auto parse(istream_t & s) { return parser<definition_t, istream_t>::parse(s); }

    }
}

#endif
