#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>  
#include <cassert>
#include <iostream>
#include <set>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "../app/use_cases.h"
#include "../menu/menu.h"
#include "../domain/book.h"
using namespace std::literals;
namespace ph = std::placeholders;


namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction(  //
        "AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1)
        // либо
        // [this](auto& cmd_input) { return AddAuthor(cmd_input); }
    );
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBook"s, {}, "Show book info"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteBook"s, "title"s, "Deletes book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "title"s, "Edit existing book"s, std::bind(&View::EditBook, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, {}, "Deletes author"s,
                std::bind(&View::DeleteAuthor, this, ph::_1));

    menu_.AddAction("EditAuthor"s, {}, "Edits author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }

        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string arg;
        std::getline(cmd_input, arg);
        boost::algorithm::trim(arg);
        
        const auto authors = use_cases_.GetAuthors();
        if (!arg.empty()) {
            std::istringstream iss(arg);
            int index;
            if ((iss >> index) && index >= 1 && index <= static_cast<int>(authors.size())) {
                if (!use_cases_.DeleteAuthorById(authors[index - 1].id)) {
                    output_ << "Failed to delete author"sv << std::endl;
                }
                return true;
            }

            if (!use_cases_.DeleteAuthorByName(arg)) {
                output_ << "Failed to delete author"sv << std::endl;
            }
            return true;
        }

        if (authors.empty()) return true;

        try {
            auto author_id = SelectAuthor();
            if (!author_id) return true;
            if (!use_cases_.DeleteAuthorById(*author_id)) {
                output_ << "Failed to delete author"sv << std::endl;
            }
        } catch (...) {
            output_ << "Failed to delete author"sv << std::endl;
        }

    } catch (...) {
        output_ << "Failed to delete author"sv << std::endl;
    }

    return true;
}



bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string arg;
        std::getline(cmd_input, arg);
        boost::algorithm::trim(arg);

        const auto authors = use_cases_.GetAuthors();
        std::optional<std::string> author_id;

        if (!arg.empty()) {
            std::istringstream iss(arg);
            int index;
            if ((iss >> index) && index >= 1 && index <= static_cast<int>(authors.size())) {
                author_id = authors[index - 1].id;
            } else {
                author_id = use_cases_.FindAuthorIdByName(arg);
            }

            if (!author_id) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
        } else {
            if (authors.empty()) return true;
            try {
                author_id = SelectAuthor();
                if (!author_id) return true;
            } catch (...) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
        }

        output_ << "Enter new name:"sv << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);

        if (new_name.empty() || !use_cases_.EditAuthor(*author_id, new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
        }
    } catch (...) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}


bool View::AddBook(std::istream& cmd_input) const {
    try {
        std::string arg;
        std::getline(cmd_input, arg);
        boost::algorithm::trim(arg);

        if (arg.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        std::string year_str;
        std::string title;

        std::istringstream iss(arg);
        iss >> year_str;
        std::getline(iss, title);
        boost::algorithm::trim(title);

        if (year_str.empty() || title.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        output_ << "Enter author name or empty line to select from list:"sv << std::endl;
        std::string author_input;
        std::getline(input_, author_input);
        boost::algorithm::trim(author_input);

        std::optional<std::string> author_id;
        auto authors = use_cases_.GetAuthors();

        if (!author_input.empty()) {
            std::istringstream aiss(author_input);
            int index;
            if ((aiss >> index) && index >= 1 && index <= static_cast<int>(authors.size())) {
                author_id = authors[index - 1].id;
            } else {
                for (const auto& a : authors) {
                    if (a.name == author_input) {
                        author_id = a.id;
                        break;
                    }
                }
                if (!author_id) {
                    output_ << "No author found. Do you want to add "sv << author_input << " (y/n)?"sv << std::endl;
                    std::string answer;
                    std::getline(input_, answer);
                    if (answer != "y" && answer != "Y") {
                        output_ << "Failed to add book"sv << std::endl;
                        return true;
                    }
                    use_cases_.AddAuthor(author_input);
                    author_id = use_cases_.FindAuthorIdByName(author_input);
                }
            }
        } else {
            try {
                author_id = SelectAuthor();
                if (!author_id) return true;
            } catch (...) {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
        }

        if (!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        output_ << "Enter tags (comma separated):"sv << std::endl;
        std::string tag_line;
        std::getline(input_, tag_line);

        std::vector<std::string> raw_tags;
        boost::algorithm::split(raw_tags, tag_line, boost::algorithm::is_any_of(","));

        std::set<std::string> tag_set;
        for (auto& tag : raw_tags) {
            boost::algorithm::trim(tag);
            while (tag.find("  ") != std::string::npos) {
                boost::algorithm::replace_all(tag, "  ", " ");
            }
            if (!tag.empty()) {
                tag_set.insert(std::move(tag));
            }
        }

        int year;
        try {
            year = std::stoi(year_str);
        } catch (...) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        use_cases_.AddBook(std::move(title), *author_id, year,
                           std::vector<std::string>(tag_set.begin(), tag_set.end()));
    } catch (...) {
        output_ << "Failed to add book"sv << std::endl;
    }

    return true;
}





bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}
bool View::ShowBook(std::istream& cmd_input) const {
    std::string raw_arg;
    std::getline(cmd_input, raw_arg);
    boost::algorithm::trim(raw_arg);

    std::string title = raw_arg;

    std::vector<domain::BookWithAuthor> candidates;

    if (!title.empty()) {
        candidates = use_cases_.FindBooksByTitle(title);
    } else {
        candidates = use_cases_.GetBooksWithAuthors();
    }

    if (candidates.empty()) {
        return true;
    }

    const domain::BookWithAuthor* selected_book = nullptr;

    if (candidates.size() > 1) {
        PrintVector(output_, candidates);
        output_ << "Enter the book # or empty line to cancel:"sv << std::endl;

        std::string input;
        std::getline(input_, input);
        boost::algorithm::trim(input);
        if (input.empty()) return true;

        std::istringstream iss(input);
        int index;
        if (!(iss >> index) || index < 1 || index > static_cast<int>(candidates.size())) {
            return true;
        }

        selected_book = &candidates[index - 1];
    } else {
        selected_book = &candidates.front();
    }

    output_ << "Title: " << selected_book->title << std::endl;
    output_ << "Author: " << selected_book->author_name << std::endl;
    output_ << "Publication year: " << selected_book->publication_year << std::endl;

    auto tags = use_cases_.GetBookTags(selected_book->id);
    if (!tags.empty()) {
        std::sort(tags.begin(), tags.end());
        output_ << "Tags: ";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) output_ << ", ";
            output_ << tags[i];
        }
        output_ << std::endl;
    }

    return true;
}




bool View::ShowBooks() const {
    auto books = GetBooksWithAuthors();
    std::sort(books.begin(), books.end(), [](const auto& a, const auto& b) {
        if (a.title != b.title)
            return a.title < b.title;
        if (a.author_name != b.author_name)
            return a.author_name < b.author_name;
        return a.publication_year < b.publication_year;
    });
    PrintVector(output_, books);
    return true;
}


bool View::ShowAuthorBooks() const {
    // TODO: handle errorDeleteBook
    try {
        if (auto author_id = SelectAuthor()) {
            PrintVector(output_, GetAuthorBooks(*author_id));
        }
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}
bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<domain::BookWithAuthor> candidates;

        if (!title.empty()) {
            candidates = use_cases_.FindBooksByTitle(title);
        } else {
            candidates = use_cases_.GetBooksWithAuthors();
        }

        if (candidates.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        const domain::BookWithAuthor* selected_book = nullptr;

        if (candidates.size() > 1) {
            PrintVector(output_, candidates);
            output_ << "Enter the book # or empty line to cancel:"sv << std::endl;
            std::string input;
            std::getline(input_, input);
            boost::algorithm::trim(input);
            if (input.empty()) return true;

            std::istringstream iss(input);
            int index;
            if (!(iss >> index) || index < 1 || index > static_cast<int>(candidates.size())) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }

            selected_book = &candidates[index - 1];
        } else {
            selected_book = &candidates.front();
        }

        if (!use_cases_.DeleteBook(selected_book->id)) {
            output_ << "Book not found"sv << std::endl;
        }

    } catch (...) {
        output_ << "Book not found"sv << std::endl;
    }

    return true;
}


bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<domain::BookWithAuthor> candidates;

        if (!title.empty()) {
            candidates = use_cases_.FindBooksByTitle(title);
        } else {
            candidates = use_cases_.GetBooksWithAuthors();
        }

        if (candidates.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        // ✅ ВСЕГДА выводим список и спрашиваем номер
        PrintVector(output_, candidates);
        output_ << "Enter the book # or empty line to cancel:"sv << std::endl;
        std::string input;
        std::getline(input_, input);
        boost::algorithm::trim(input);
        if (input.empty()) return true;

        std::istringstream iss(input);
        int index;
        if (!(iss >> index) || index < 1 || index > static_cast<int>(candidates.size())) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        const auto& book = candidates[index - 1];

        output_ << "Enter new title or empty line to use the current one (" << book.title << "):"sv << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) new_title = book.title;

        output_ << "Enter publication year or empty line to use the current one (" << book.publication_year << "):"sv << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        int new_year = book.publication_year;
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
        }

        output_ << "Enter tags (current tags: "sv;
        auto tags = use_cases_.GetBookTags(book.id);
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) output_ << ", ";
            output_ << tags[i];
        }
        output_ << "):"sv << std::endl;

        std::string tag_line;
        std::getline(input_, tag_line);
        std::vector<std::string> raw_tags;
        boost::algorithm::split(raw_tags, tag_line, boost::algorithm::is_any_of(","));

        std::set<std::string> tag_set;
        for (auto& tag : raw_tags) {
            boost::algorithm::trim(tag);
            while (tag.find("  ") != std::string::npos) {
                boost::algorithm::replace_all(tag, "  ", " ");
            }
            if (!tag.empty()) {
                tag_set.insert(std::move(tag));
            }
        }

        if (!use_cases_.EditBook(book.id, new_title, new_year,
                                 std::vector<std::string>(tag_set.begin(), tag_set.end()))) {
            output_ << "Book not found"sv << std::endl;
        }
    } catch (...) {
        output_ << "Book not found"sv << std::endl;
    }

    return true;
}





std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    cmd_input >> params.publication_year;
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);

    auto author_id = SelectAuthor();
    if (not author_id.has_value())
        return std::nullopt;
    else {
        params.author_id = author_id.value();
        return params;
    }
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= authors.size()) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    return use_cases_.GetAuthors();
}
std::vector<detail::BookInfo> View::GetBooks() const {
    return use_cases_.GetBooks();
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    return use_cases_.GetAuthorBooks(author_id);
}
std::vector<domain::BookWithAuthor> View::GetBooksWithAuthors() const {
    return use_cases_.GetBooksWithAuthors();
}

}  // namespace ui
namespace domain{
std::ostream& operator<<(std::ostream& out, const domain::BookWithAuthor& book) {
    return out << book.title << " by " << book.author_name << ", " << book.publication_year;
}

}
