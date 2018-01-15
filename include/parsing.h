/**
 * @file xml.h
 * @author Chase Geigle
 *
 * Helper classes for XML parsing for the StackExchange dumps.
 */

#ifndef STACKEXCHANGE_PARSING_H_
#define STACKEXCHANGE_PARSING_H_

#include <libxml/xmlreader.h>

#include "date.h"
#include "meta/io/xzstream.h"
#include "meta/util/progress.h"
#include "meta/util/string_view.h"
#include "meta/util/optional.h"

class xml_string
{
  public:
    xml_string(xmlChar* str) : str_{str}
    {
        // nothing
    }

    ~xml_string()
    {
        ::free(str_);
    }

    xml_string(xml_string&& other) : str_(other.str_)
    {
        other.str_ = nullptr;
    }

    xml_string& operator=(xml_string&& other)
    {
        str_ = other.str_;
        other.str_ = nullptr;
        return *this;
    }

    xml_string(const xml_string&) = delete;
    xml_string& operator=(const xml_string&) = delete;

    std::string to_string() const
    {
        return {(const char*)str_};
    }

    meta::util::string_view sv() const
    {
        return {(const char*)str_};
    }

  private:
    xmlChar* str_;
};

class xml_text_reader
{
  public:
    xml_text_reader(meta::io::xzifstream& input,
                    meta::printing::progress& progress)
        : input_(input), progress_(progress), reader_{make_xml_reader()}
    {
    }

    bool read_next()
    {
        return xmlTextReaderRead(reader_);
    }

    meta::util::string_view node_name() const
    {
        return (const char*)xmlTextReaderConstName(reader_);
    }

    meta::util::optional<xml_string> attribute(const char* name) const
    {
        auto str = xmlTextReaderGetAttribute(reader_, (const xmlChar*)name);
        if (!str)
            return meta::util::nullopt;
        return {str};
    }

    operator xmlTextReaderPtr()
    {
        return reader_;
    }

  private:
    xmlTextReaderPtr make_xml_reader()
    {
        return xmlReaderForIO(
            // Read callback
            [](void* context, char* buffer, int len) {
                auto self = static_cast<xml_text_reader*>(context);
                self->input_.read(buffer, len);

                auto num_read = static_cast<int>(self->input_.gcount());
                self->progress_(self->input_.bytes_read());
                return num_read;
            },
            // Close callback
            [](void* /* context */) { return 0; },
            // Context
            this,
            // URL
            "",
            // Encoding
            nullptr,
            // Options
            XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_RECOVER);
    }

    meta::io::xzifstream& input_;
    meta::printing::progress& progress_;
    xmlTextReaderPtr reader_;
};

using sys_milliseconds = date::sys_time<std::chrono::milliseconds>;

inline sys_milliseconds parse_date(const std::string& date)
{
    std::stringstream ss{date};
    sys_milliseconds tp;
    ss >> date::parse("%Y-%m-%dT%H:%M:%S", tp);
    if (ss.fail())
        throw std::runtime_error{"failed to parse date: " + date};
    return tp;
}

struct time_span
{
    sys_milliseconds earliest;
    sys_milliseconds latest;

    void update(sys_milliseconds timestamp)
    {
        if (earliest > timestamp)
            earliest = timestamp;
        if (latest < timestamp)
            latest = timestamp;
    }

    void update(const time_span& other)
    {
        update(other.earliest);
        update(other.latest);
    }
};

#endif
