#include "ProductController.h"
#include "AuthMiddleware.h"
#include "MultipartParser.h"
#include <random>
#include <fstream>
#include <sstream>
using json = nlohmann::json;


void ProductController::register_routes(Router& router)
{
    router.add(
        "GET",
        "/products",
        [this](const Request& req)
        {
            return get_products(req);
        }
    );
    router.add("GET", "/products/search", [this](const Request& req)
        {
            std::cerr << ">>> SEARCH PRODUCTS HANDLER\n";
            return search_products(req);
        });

    router.add(
        "GET",
        "/products/:id",
        [this](const Request& req)
        {   
            std::cerr << ">>> GET PRODUCT HANDLER, path = ";
            return get_product(req);
        }
    );

    router.add(
        "POST",
        "/products",
        [this](const Request& req)
        {
            return add_product(req);
        }
    );

    router.add(
        "DELETE",
        "/products/:id",
        [this](const Request& req)
        {
            return delete_product(req);
        }
    );

    router.add(
        "PUT",
        "/products/:id",
        [this](const Request& req)
        {
            return update_product(req);
        }
    );
    router.add("GET", "/products/:id/compatibility", [this](const Request& req)
        {
            return get_compatibility(req);
        });

    router.add("POST", "/products/:id/compatibility", [this](const Request& req)
        {
            return add_compatibility(req);
        });

    router.add("GET", "/products/type/:type", [this](const Request& req)
        {
            std::string type = req.params.at("type");
            return Response{ service.get_products_by_type(type), "application/json" };
        });

    router.add("POST", "/products/:id/photo", [this](const Request& req)
        {
            return upload_photo(req);
        });

    router.add("GET", "/uploads/:filename", [this](const Request& req)
        {
            return serve_upload(req);
        });


}

Response ProductController::search_products(const Request& req)
{
    ProductQuery q;

    auto it = req.params.find("search");
    if (it != req.params.end() && !it->second.empty()) q.search = it->second;

    it = req.params.find("type");
    if (it != req.params.end() && !it->second.empty()) q.type = it->second;

    it = req.params.find("sort_by");
    if (it != req.params.end()) q.sort_by = it->second;

    it = req.params.find("sort_dir");
    if (it != req.params.end()) q.sort_dir = it->second;

    it = req.params.find("limit");
    if (it != req.params.end())
    {
        try { q.limit = std::clamp(std::stoi(it->second), 1, 100); }
        catch (...) {}
    }

    it = req.params.find("offset");
    if (it != req.params.end())
    {
        try { q.offset = max(0, std::stoi(it->second)); }
        catch (...) {}
    }
    it = req.params.find("make");
    if (it != req.params.end() && !it->second.empty()) q.make = it->second;

    it = req.params.find("model");
    if (it != req.params.end() && !it->second.empty()) q.model = it->second;

    it = req.params.find("year");
    if (it != req.params.end())
    {
        try { q.year = std::stoi(it->second); }
        catch (...) {}
    }

    return Response{ service.search_products(q), "application/json" };
}
Response ProductController::get_products(const Request& req)
{
    return Response{
        service.get_products(),
        "application/json"
    };
}


Response ProductController::get_product(const Request& req)
{
    try
    {
        int id = std::stoi(req.params.at("id"));

        return Response{
            service.get_product(id),
            "application/json"
        };
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }
}


    Response ProductController::add_product(const Request& req)
    {
        auto auth = require_roles(req, sessions, { "admin", "employee" });
        if (!auth.ok) return auth.error;

        try
        {
            auto data = nlohmann::json::parse(req.body);

            std::string type =
                data.at("type").get<std::string>();

            std::string name =
                data.at("name").get<std::string>();
            int sku = data.at("sku").get<int>();
            std::string image_url = data.at("image_url").get<std::string>();

            double cost =
                data.at("cost").get<double>();

            double price =
                data.at("price").get<double>();

            int stock =
                data.at("stock_quantity").get<int>();

            return Response{
                service.add_product(
                    type,
                    name,
                    cost,
                    price,
                    stock,
                    sku,
                    image_url
                ),
                "application/json",
                "201 Created"
            };
        }
        catch (...)
        {
            return Response{
                R"({"error":"Invalid JSON or missing fields"})",
                "application/json",
                "400 Bad Request"
            };
        }
    }


Response ProductController::delete_product(const Request& req)
{
    auto auth = require_roles(req, sessions, {"admin"});

    if (!auth.ok)
    {
        return auth.error;
    }

    int id;

    try
    {
        id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }

    std::string result =
        service.delete_product(id);

    json result_json;

    try
    {
        result_json = json::parse(result);
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid service response"})",
            "application/json",
            "500 Internal Server Error"
        };
    }

    std::string status_code = "200 OK";

    if (result_json.contains("error"))
    {
        if (result_json["error"] == "Product not found")
        {
            status_code = "404 Not Found";
        }
        else
        {
            status_code = "400 Bad Request";
        }
    }

    return Response{
        result_json.dump(),
        "application/json",
        status_code
    };
}


Response ProductController::update_product(const Request& req)
{

    auto auth = require_roles(req, sessions, {"admin", "employee"});
    if (!auth.ok) return auth.error;
    int id;

    try
    {
        id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }

    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid JSON"})",
            "application/json",
            "400 Bad Request"
        };
    }

    try
    {
        double price =
            body.at("price").get<double>();

        int stock =
            body.at("stock_quantity").get<int>();

        std::string result =
            service.update_product(
                id,
                price,
                stock
            );

        json result_json =
            json::parse(result);

        std::string status_code = "200 OK";

        if (result_json.contains("error"))
        {
            if (result_json["error"] == "Product not found")
            {
                status_code = "404 Not Found";
            }
            else
            {
                status_code = "400 Bad Request";
            }
        }

        return Response{
            result_json.dump(),
            "application/json",
            status_code
        };
    }
    catch (...)
    {
        return Response{
            R"({"error":"Missing or invalid fields: price, stock_quantity"})",
            "application/json",
            "400 Bad Request"
        };
    }
}
Response ProductController::add_compatibility(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin", "employee" });
    if (!auth.ok) return auth.error;

    int product_id;
    try { product_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid product id"})", "application/json", "400 Bad Request" }; }

    json body;
    try { body = json::parse(req.body); }
    catch (...) { return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" }; }

    std::string make, model;
    std::optional<int> year_from, year_to;

    try
    {
        make = body.at("make").get<std::string>();
        model = body.at("model").get<std::string>();
        if (body.contains("year_from")) year_from = body.at("year_from").get<int>();
        if (body.contains("year_to")) year_to = body.at("year_to").get<int>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing make or model"})", "application/json", "400 Bad Request" };
    }

    std::string result = service.add_compatibility(product_id, make, model, year_from, year_to);
    json result_json = json::parse(result);

    return Response{ result_json.dump(), "application/json",
        result_json.contains("error") ? "500 Internal Server Error" : "201 Created" };
}

Response ProductController::get_compatibility(const Request& req)
{
    int product_id;
    try { product_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid product id"})", "application/json", "400 Bad Request" }; }

    return Response{ service.get_compatibility(product_id), "application/json" };
}

static bool is_allowed_image_type(const std::string& content_type)
{
    return content_type == "image/jpeg" || content_type == "image/png" || content_type == "image/webp";
}

static std::string extension_for(const std::string& content_type)
{
    if (content_type == "image/png") return ".png";
    if (content_type == "image/webp") return ".webp";
    return ".jpg";
}

static std::string generate_filename(const std::string& ext)
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << std::hex << dist(gen) << ext;
    return oss.str();
}

Response ProductController::upload_photo(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin", "employee" });
    if (!auth.ok) return auth.error;

    int product_id;
    try { product_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid product id"})", "application/json", "400 Bad Request" }; }

    auto content_type_it = req.headers.find("content-type");
    if (content_type_it == req.headers.end() || content_type_it->second.find("multipart/form-data") == std::string::npos)
    {
        return Response{ R"({"error":"Expected multipart/form-data"})", "application/json", "400 Bad Request" };
    }

    auto boundary = extract_boundary(content_type_it->second);
    if (!boundary)
    {
        return Response{ R"({"error":"Missing boundary"})", "application/json", "400 Bad Request" };
    }

    auto files = parse_multipart(req.body, *boundary);

    if (files.empty())
    {
        return Response{ R"({"error":"No file uploaded"})", "application/json", "400 Bad Request" };
    }

    const UploadedFile& file = files[0];

    if (!is_allowed_image_type(file.content_type))
    {
        return Response{ R"({"error":"Only JPEG, PNG or WEBP images are allowed"})", "application/json", "400 Bad Request" };
    }

    const size_t max_file_size = 5 * 1024 * 1024; // 5 МБ на файл
    if (file.data.size() > max_file_size)
    {
        return Response{ R"({"error":"File too large, max 5 MB"})", "application/json", "400 Bad Request" };
    }

    std::filesystem::create_directories("uploads");

    std::string ext = extension_for(file.content_type);
    std::string saved_name = generate_filename(ext);
    std::string saved_path = "uploads/" + saved_name;

    std::ofstream out(saved_path, std::ios::binary);
    if (!out.is_open())
    {
        return Response{ R"({"error":"Failed to save file"})", "application/json", "500 Internal Server Error" };
    }

    out.write(file.data.data(), file.data.size());
    out.close();

    std::string image_url = "/uploads/" + saved_name;

    std::string result = service.update_product_image(product_id, image_url);
    json result_json = json::parse(result);

    if (result_json.contains("error"))
    {
        std::filesystem::remove(saved_path); // не оставляем файл-сироту, если запись в БД не удалась
        return Response{ result_json.dump(), "application/json", "404 Not Found" };
    }

    json response = { {"status", "photo uploaded"}, {"image_url", image_url} };
    return Response{ response.dump(), "application/json" };
}

Response ProductController::serve_upload(const Request& req)
{
    std::string filename = req.params.at("filename");

    // защита от выхода за пределы папки uploads (../../etc/passwd и подобное)
    if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
    {
        return Response{ "Bad Request", "text/plain", "400 Bad Request" };
    }

    std::string path = "uploads/" + filename;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return Response{ "Not Found", "text/plain", "404 Not Found" };
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::string content_type = "image/jpeg";
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".png") content_type = "image/png";
    if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".webp") content_type = "image/webp";

    return Response{ buffer.str(), content_type };
}