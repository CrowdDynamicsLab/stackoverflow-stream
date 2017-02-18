/**
 * @file repack.cpp
 * @author Chase Geigle
 *
 * Repackages a .7z archive from a StackOverflow data dump into one (or
 * more) .xz compressed files on disk. This allows for more easy streaming
 * by my other tools.
 */

#include <archive.h>
#include <archive_entry.h>
#include <iostream>

#include "meta/io/filesystem.h"
#include "meta/io/xzstream.h"
#include "meta/logging/logger.h"
#include "meta/util/progress.h"

class archive_ptr
{
  public:
    archive_ptr() : a_{archive_read_new()}
    {
        if (!a_)
            throw std::runtime_error{"failed to construct archive"};
    }

    operator archive*()
    {
        return a_;
    }

    ~archive_ptr()
    {
        archive_read_free(a_);
    }

  private:
    archive* a_;
};

struct archive_file
{
  public:
    archive_file(const char* filename)
    {
        archive_read_support_format_all(a_);
        archive_read_support_filter_all(a_);

        if (archive_read_open_filename(a_, filename, 10240))
        {
            throw std::runtime_error{"failed to open file: "
                                     + std::string{filename}};
        }
    }

    operator archive*()
    {
        return a_;
    }

  private:
    archive_ptr a_;
};

void repack_file(archive_file& file, archive_entry* entry,
                 const std::string& folder)
{
    using namespace meta;
    auto path = archive_entry_pathname(entry);
    auto filesize = archive_entry_size(entry);
    auto full_path = folder + "/" + path + ".xz";

    printing::progress progress{" > Repacking " + std::string{path} + ": ",
                                static_cast<uint64_t>(filesize)};

    io::xzofstream output{full_path};

    char buff[10240];
    uint64_t bytes = 0;
    while (true)
    {
        auto res = archive_read_data(file, buff, sizeof(buff));
        if (res < 0)
            throw std::runtime_error{std::string{"failed to extract file: "}
                                     + archive_error_string(file)};

        if (res == 0)
            break;

        progress(bytes += static_cast<uint64_t>(res));

        output.write(buff, res);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " filename.7z [filename2.7z...]"
                  << std::endl;
        return 1;
    }

    using namespace meta;

    logging::set_cerr_logging();

    for (int i = 1; i < argc; ++i)
    {
        archive_file file{argv[i]};
        archive_entry* entry;

        std::string folder{argv[i]};
        auto pos = folder.rfind('.');
        if (pos != folder.npos)
            folder.erase(pos, folder.length() - pos);

        LOG(progress) << "Repacking " << folder << "...\n" << ENDLG;

        filesystem::make_directories("repacked/" + folder);
        while (true)
        {
            int res = archive_read_next_header(file, &entry);
            if (res == ARCHIVE_EOF)
                break;

            if (res != ARCHIVE_OK)
                throw std::runtime_error{std::string{"error reading archive: "}
                                         + archive_error_string(file)};

            if (archive_entry_size(entry) > 0)
                repack_file(file, entry, "repacked/" + folder);
        }
    }

    return 0;
}
