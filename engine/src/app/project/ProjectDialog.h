#pragma once

#include "api/state/ProjectState.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace ihub::app
{

    class ProjectDialog
    {
    public:
        ProjectDialog () = default;
        ~ProjectDialog ();

        ProjectDialog (const ProjectDialog&) = delete;
        ProjectDialog& operator= (const ProjectDialog&) = delete;

        bool
        begin (const Api::ProjectDialogRequest& request, std::uint64_t& id, std::string& error);

        bool poll (std::uint64_t id, Api::ProjectDialogStatus& status, std::string& error) const;

    private:
        struct Shared;

        mutable std::mutex mutex;
        std::shared_ptr<Shared> current;
        std::uint64_t nextId = 1;
    };

}
