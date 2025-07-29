#include "use_cases_impl.h"
#include <tuple>
#include "../domain/author.h"
#include "../domain/book.h"
namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}
std::vector<ui::detail::AuthorInfo> UseCasesImpl::GetAuthors() const {
    std::vector<ui::detail::AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
	ui::detail::AuthorInfo info;
	info.id = author.GetId().ToString();
	info.name = author.GetName();
	result.push_back(std::move(info));

    }
    return result;
}
void UseCasesImpl::AddBook(std::string title, std::string author_id, int publication_year,
                           std::vector<std::string> tags) {
    const auto book_id = domain::BookId::New();
    books_.Save({book_id, domain::AuthorId::FromString(author_id), std::move(title), publication_year});

    for (const auto& tag : tags) {
        book_tags_.Save({book_id, tag});
    }
}


std::vector<ui::detail::BookInfo> UseCasesImpl::GetBooks() const {
    auto books = books_.GetAllWithAuthors();
    std::vector<ui::detail::BookInfo> result;
    for (const auto& b : books) {
        ui::detail::BookInfo info;
        info.id = b.id.ToString();
        info.title = b.title;
        info.author_name = b.author_name;
        info.publication_year = b.publication_year;
        result.push_back(std::move(info));

    }

    // Сортировка: title → author_name → publication_year
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return std::tie(a.title, a.author_name, a.publication_year) <
               std::tie(b.title, b.author_name, b.publication_year);
    });

    return result;
}

std::vector<ui::detail::BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<ui::detail::BookInfo> result;
    AuthorId id = AuthorId::FromString(author_id);
    for (const auto& book : books_.GetByAuthor(id)) {
        ui::detail::BookInfo info;
        info.id = book.GetId().ToString();
        info.title = book.GetTitle();
        auto author = authors_.FindById(book.GetAuthorId());
        info.author_name = author ? author->GetName() : "";
        info.publication_year = book.GetPublicationYear();
        result.push_back(std::move(info));
    }
    return result;
}

bool UseCasesImpl::DeleteAuthorById(const std::string& id) {
    try {
        const auto author_id = domain::AuthorId::FromString(id);
        const auto books = books_.GetByAuthor(author_id);

        for (const auto& book : books) {
            book_tags_.DeleteByBookId(book.GetId());
            books_.Delete(book.GetId());
        }

        return authors_.DeleteById(author_id);
    } catch (...) {
        return false;
    }
}
bool UseCasesImpl::DeleteAuthorByName(const std::string& name) {
    try {
        auto author_id = authors_.FindIdByName(name);
        if (!author_id) {
            return false;
        }
        return DeleteAuthorById(author_id->ToString());
    } catch (...) {
        return false;
    }
}
std::vector<domain::BookWithAuthor> UseCasesImpl::FindBooksByTitle(const std::string& title) const {
    return books_.FindBooksByTitle(title);
}

bool UseCasesImpl::DeleteBook(const domain::BookId& id) {
    return books_.DeleteBook(id);
}

bool UseCasesImpl::EditBook(const domain::BookId& id, const std::string& title, int year, const std::vector<std::string>& tags) {
    return books_.EditBook(id, title, year, tags);
}

std::vector<std::string> UseCasesImpl::GetBookTags(const domain::BookId& id) const {
    return book_tags_.GetTags(id);
}

std::vector<domain::BookWithAuthor> UseCasesImpl::GetBooksWithAuthors() const {
    return books_.GetAllWithAuthors();
}

std::optional<std::string> UseCasesImpl::FindAuthorIdByName(const std::string& name) const {
    auto id = authors_.FindIdByName(name);
    if (id) return id->ToString();
    return std::nullopt;
}
bool UseCasesImpl::EditAuthor(const std::string& id, const std::string& new_name) {
    try {
        return authors_.UpdateName(domain::AuthorId::FromString(id), new_name);
    } catch (...) {
        return false;
    }
}
}
