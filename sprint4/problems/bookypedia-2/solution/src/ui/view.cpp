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
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteBook"s, {}, "Deletes book"s, std::bind(&View::DeleteBook, this));
    menu_.AddAction("EditBook"s, {}, "Edit existing book"s, std::bind(&View::EditBook, this));
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
        std::string input_line;
        std::getline(cmd_input, input_line);
        boost::algorithm::trim(input_line);

        if (!input_line.empty()) {
            // Попробуем распарсить как индекс
            std::istringstream iss(input_line);
            int index;
            if ((iss >> index) && index >= 1) {
                const auto authors = use_cases_.GetAuthors();
                if (index <= static_cast<int>(authors.size())) {
                    if (!use_cases_.DeleteAuthorById(authors[index - 1].id)) {
                        output_ << "Failed to delete author"sv << std::endl;
                    }
                    return true;
                }
            }

            // Если не число — попробуем удалить по имени
            if (!use_cases_.DeleteAuthorByName(input_line)) {
                output_ << "Failed to delete author"sv << std::endl;
            }
            return true;
        }

        const auto authors = use_cases_.GetAuthors();
        if (authors.empty()) return true;

        PrintVector(output_, authors);
        output_ << "Enter author # or empty line to cancel"sv << std::endl;
        std::string input;
        std::getline(cmd_input, input);
        if (input.empty()) return true;

        std::istringstream iss(input);
        int index;
        if (!(iss >> index) || index < 1 || index > static_cast<int>(authors.size())) {
            output_ << "Failed to delete author"sv << std::endl;
            return true;
        }

        if (!use_cases_.DeleteAuthorById(authors[index - 1].id)) {
            output_ << "Failed to delete author"sv << std::endl;
        }

    } catch (...) {
        output_ << "Failed to delete author"sv << std::endl;
    }

    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string input_line;
        std::getline(cmd_input, input_line);
        boost::algorithm::trim(input_line);

        std::optional<std::string> author_id;

        if (!input_line.empty()) {
            // сначала пробуем по имени
            author_id = use_cases_.FindAuthorIdByName(input_line);

            // потом пробуем как индекс
            if (!author_id) {
                std::istringstream iss(input_line);
                int index;
                if ((iss >> index) && index >= 1) {
                    const auto authors = use_cases_.GetAuthors();
                    if (index <= static_cast<int>(authors.size())) {
                        author_id = authors[index - 1].id;
                    }
                }
            }

            if (!author_id) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }

        } else {
            const auto authors = use_cases_.GetAuthors();
            if (authors.empty()) return true;

            output_ << "Select author:"sv << std::endl;
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel"sv << std::endl;

            std::string input;
            std::getline(cmd_input, input);
            if (input.empty()) return true;

            std::istringstream iss(input);
            int index;
            if (!(iss >> index) || index < 1 || index > static_cast<int>(authors.size())) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }

            author_id = authors[index - 1].id;
        }

        output_ << "Enter new name:"sv << std::endl;
        std::string new_name;
        std::getline(cmd_input, new_name);
        boost::algorithm::trim(new_name);

        if (!use_cases_.EditAuthor(*author_id, new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
        }

    } catch (...) {
        output_ << "Failed to edit author"sv << std::endl;
    }

    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        std::string year_str, title;

        std::getline(cmd_input, year_str);
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        output_ << "Enter author name or empty line to select from list:"sv << std::endl;
        std::string author_name;
        std::getline(cmd_input, author_name);
        boost::algorithm::trim(author_name);

        std::optional<std::string> author_id;

        if (!author_name.empty()) {
            auto authors = use_cases_.GetAuthors();
            for (const auto& author : authors) {
                if (author.name == author_name) {
                    author_id = author.id;
                    break;
                }
            }

            if (!author_id) {
                output_ << "No author found. Do you want to add "sv << author_name << " (y/n)?"sv << std::endl;
                std::string answer;
                std::getline(cmd_input, answer);
                if (answer != "y" && answer != "Y") {
                    output_ << "Failed to add book"sv << std::endl;
                    return true;
                }
                use_cases_.AddAuthor(author_name);
                // ищем id добавленного автора
                auto authors2 = use_cases_.GetAuthors();
                for (const auto& author : authors2) {
                    if (author.name == author_name) {
                        author_id = author.id;
                        break;
                    }
                }
            }
        } else {
            const auto authors = use_cases_.GetAuthors();
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel"sv << std::endl;
            std::string input;
            std::getline(cmd_input, input);
            if (input.empty()) return true;

            std::istringstream iss(input);
            int index;
            if (!(iss >> index) || index < 1 || index > static_cast<int>(authors.size())) {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            author_id = authors[index - 1].id;
        }

        output_ << "Enter tags (comma separated):"sv << std::endl;
        std::string tag_line;
        std::getline(cmd_input, tag_line);

        std::vector<std::string> raw_tags;
        boost::algorithm::split(raw_tags, tag_line, boost::algorithm::is_any_of(","));

        std::set<std::string> tag_set;
        for (auto& tag : raw_tags) {
            boost::algorithm::trim(tag);
            boost::algorithm::replace_all(tag, "  ", " ");
            while (tag.find("  ") != std::string::npos) {
                boost::algorithm::replace_all(tag, "  ", " ");
            }
            if (!tag.empty()) {
                tag_set.insert(std::move(tag));
            }
        }

        int year = std::stoi(year_str);
        use_cases_.AddBook(std::move(title), *author_id, year, {tag_set.begin(), tag_set.end()});

    } catch (...) {
        output_ << "Failed to add book"sv << std::endl;
    }

    return true;
}



bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, GetBooksWithAuthors());

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
bool View::DeleteBook() const {
    try {
        output_ << "Enter book title or empty line to select from list:"sv << std::endl;
        std::string title;
        std::getline(input_, title);
        boost::algorithm::trim(title);

        std::vector<domain::BookWithAuthor> candidates;
        if (!title.empty()) {
            candidates = use_cases_.FindBooksByTitle(title);
            if (candidates.empty()) {
                output_ << "Failed to delete book"sv << std::endl;
                return true;
            }
        } else {
            candidates = use_cases_.GetBooksWithAuthors();
            if (candidates.empty()) return true;
        }

        if (candidates.size() > 1) {
            PrintVector(output_, candidates);
            output_ << "Enter the book # or empty line to cancel:"sv << std::endl;
            std::string input;
            std::getline(input_, input);
            if (input.empty()) return true;

            int idx = std::stoi(input);
            if (idx < 1 || idx > static_cast<int>(candidates.size())) {
                output_ << "Failed to delete book"sv << std::endl;
                return true;
            }
            if (!use_cases_.DeleteBook(candidates[idx - 1].id)) {
                output_ << "Failed to delete book"sv << std::endl;
            }
        } else {
            if (!use_cases_.DeleteBook(candidates.front().id)) {
                output_ << "Failed to delete book"sv << std::endl;
            }
        }
    } catch (...) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook() const {
    try {
        output_ << "Enter book title or empty line to select from list:"sv << std::endl;
        std::string title;
        std::getline(input_, title);
        boost::algorithm::trim(title);

        std::vector<domain::BookWithAuthor> candidates;
        if (!title.empty()) {
            candidates = use_cases_.FindBooksByTitle(title);
            if (candidates.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
        } else {
            candidates = use_cases_.GetBooksWithAuthors();
            if (candidates.empty()) return true;
        }

        const domain::BookWithAuthor* book = nullptr;

        if (candidates.size() > 1) {
            PrintVector(output_, candidates);
            output_ << "Enter the book # or empty line to cancel:"sv << std::endl;
            std::string input;
            std::getline(input_, input);
            if (input.empty()) return true;

            int idx = std::stoi(input);
            if (idx < 1 || idx > static_cast<int>(candidates.size())) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            book = &candidates[idx - 1];
        } else {
            book = &candidates.front();
        }

        output_ << "Enter new title or empty line to use the current one (" << book->title << "):"sv << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) new_title = book->title;

        output_ << "Enter publication year or empty line to use the current one (" << book->publication_year << "):"sv << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        int new_year = book->publication_year;
        if (!year_str.empty()) {
            new_year = std::stoi(year_str);
        }

        output_ << "Enter tags (current tags: "sv;
        auto tags = use_cases_.GetBookTags(book->id);
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
            boost::algorithm::replace_all(tag, "  ", " ");
            while (tag.find("  ") != std::string::npos) {
                boost::algorithm::replace_all(tag, "  ", " ");
            }
            if (!tag.empty()) tag_set.insert(std::move(tag));
        }

        if (!use_cases_.EditBook(book->id, new_title, new_year, {tag_set.begin(), tag_set.end()})) {
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
