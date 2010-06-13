/**
    @file

    Tests for CProvider.

    @if licence

    Copyright (C) 2010  Alexander Lamaison <awl03@doc.ic.ac.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    In addition, as a special exception, the the copyright holders give you
    permission to combine this program with free software programs or the 
    OpenSSL project's "OpenSSL" library (or with modified versions of it, 
    with unchanged license). You may copy and distribute such a system 
    following the terms of the GNU GPL for this program and the licenses 
    of the other code concerned. The GNU General Public License gives 
    permission to release a modified version without this exception; this 
    exception also makes it possible to release a modified version which 
    carries forward this exception.

    @endif
*/

#include "swish/interfaces/SftpProvider.h" // ISftpProvider

#include "test/common_boost/helpers.hpp"
#include "test/common_boost/ProviderFixture.hpp" // ProviderFixture

#include <comet/bstr.h> // bstr_t
#include <comet/ptr.h> // com_ptr

#include <boost/filesystem/path.hpp> // wpath
#include <boost/filesystem/fstream.hpp> // ofstream taking a path

#include <boost/test/unit_test.hpp>

#include <string>

using test::ProviderFixture;

using comet::bstr_t;
using comet::com_ptr;

using boost::filesystem::ofstream;
using boost::filesystem::wpath;

using std::string;
using std::wstring;

namespace {

	const string longentry = 
		"-rw-r--r--    1 swish    wheel         767 Dec  8  2005 .cshrc";

	void free_listing(Listing& listing)
	{
		if (listing.bstrFilename)
			::SysFreeString(listing.bstrFilename);
		if (listing.bstrOwner)
			::SysFreeString(listing.bstrOwner);
		if (listing.bstrGroup)
			::SysFreeString(listing.bstrGroup);
	}
}

BOOST_FIXTURE_TEST_SUITE(provider_tests, ProviderFixture)

/**
 * List contents of an empty directory.
 */
BOOST_AUTO_TEST_CASE( list_empty_dir )
{
	com_ptr<ISftpProvider> provider = Provider();
	com_ptr<IEnumListing> listing;

	HRESULT hr = provider->GetListing(
		Consumer().get(), bstr_t(ToRemotePath(Sandbox()).string()).in(),
		listing.out());
	BOOST_REQUIRE_OK(hr);

	Listing item;
	ULONG fetched;
	hr = listing->Next(1, &item, &fetched);
	BOOST_CHECK(hr == S_FALSE);
	BOOST_CHECK_EQUAL(fetched, 0U);
}

/**
 * List contents of a directory.
 */
BOOST_AUTO_TEST_CASE( list_dir )
{
	wpath file1 = NewFileInSandbox();
	wpath file2 = NewFileInSandbox();

	com_ptr<ISftpProvider> provider = Provider();
	com_ptr<IEnumListing> listing;

	HRESULT hr = provider->GetListing(
		Consumer().get(), bstr_t(ToRemotePath(Sandbox()).string()).in(),
		listing.out());
	BOOST_REQUIRE_OK(hr);

	Listing item;
	ULONG fetched;
	hr = listing->Next(1, &item, &fetched);
	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK_EQUAL(fetched, 1U);
	BOOST_CHECK_EQUAL(item.bstrFilename, file1.filename());
	free_listing(item);

	hr = listing->Next(1, &item, &fetched);
	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK_EQUAL(item.bstrFilename, file2.filename());
	free_listing(item);

	hr = listing->Next(1, &item, &fetched);
	BOOST_CHECK(hr = S_FALSE);
}

/**
 * Can we handle a unicode filename?
 */
BOOST_AUTO_TEST_CASE( unicode )
{
	// create an empty file with a unicode filename in the sandbox
	wpath unicode_file_name = Sandbox() / L"русский";
	ofstream(unicode_file_name).close();
	BOOST_CHECK(exists(unicode_file_name));
	BOOST_CHECK(is_regular_file(unicode_file_name));
	BOOST_CHECK(unicode_file_name.is_complete());

	com_ptr<ISftpProvider> provider = Provider();
	com_ptr<IEnumListing> listing;

	HRESULT hr = provider->GetListing(
		Consumer().get(), bstr_t(ToRemotePath(Sandbox()).string()).in(),
		listing.out());
	BOOST_REQUIRE_OK(hr);

	Listing item;
	ULONG fetched;
	hr = listing->Next(1, &item, &fetched);
	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK_EQUAL(item.bstrFilename, unicode_file_name.filename());
	free_listing(item);
}

/**
 * Can we see inside directories whose names are non-latin Unicode?
 */
BOOST_AUTO_TEST_CASE( list_unicode_dir )
{
	wpath directory = Sandbox() / L"漢字 العربية русский 47";
	create_directory(directory);
	wpath file = directory / L"latin filename";
	ofstream(file).close();

	com_ptr<IEnumListing> listing;
	HRESULT hr = Provider()->GetListing(
		Consumer().get(), bstr_t(ToRemotePath(directory).string()).in(),
		listing.out());
	BOOST_REQUIRE_OK(hr);
}

/**
 * Rename a file.
 */
BOOST_AUTO_TEST_CASE( rename_file )
{
	wpath file = NewFileInSandbox();
	wpath renamed_file = file.parent_path() / (file.filename() + L"renamed");

	com_ptr<ISftpProvider> provider = Provider();

	bstr_t old_name(ToRemotePath(file).string());
	bstr_t new_name(ToRemotePath(renamed_file).string());

	VARIANT_BOOL was_overwritten = VARIANT_FALSE;
	HRESULT hr = provider->Rename(
		Consumer().get(), old_name.in(), new_name.in(), &was_overwritten);

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK_EQUAL(was_overwritten, VARIANT_FALSE);
	BOOST_CHECK(exists(renamed_file));
	BOOST_CHECK(!exists(file));
}

/**
 * Rename a Unicode file.
 */
BOOST_AUTO_TEST_CASE( rename_unicode_file )
{
	// create an empty file with a unicode filename in the sandbox
	wpath unicode_file_name = Sandbox() / L"русский.txt";
	ofstream(unicode_file_name).close();
	BOOST_CHECK(exists(unicode_file_name));

	wpath renamed_file = Sandbox() / L"Россия";

	com_ptr<ISftpProvider> provider = Provider();

	bstr_t old_name(ToRemotePath(unicode_file_name).string());
	bstr_t new_name(ToRemotePath(renamed_file).string());

	VARIANT_BOOL was_overwritten = VARIANT_FALSE;
	HRESULT hr = provider->Rename(
		Consumer().get(), old_name.in(), new_name.in(), &was_overwritten);

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK_EQUAL(was_overwritten, VARIANT_FALSE);
	BOOST_CHECK(exists(renamed_file));
	BOOST_CHECK(!exists(unicode_file_name));
}

/**
 * Delete a file and ensure other files in the same folder aren't also removed.
 */
BOOST_AUTO_TEST_CASE( delete_file )
{
	wpath file_before = NewFileInSandbox();
	wpath file = NewFileInSandbox();
	wpath file_after = NewFileInSandbox();

	HRESULT hr = Provider()->Delete(
		Consumer().get(), bstr_t(ToRemotePath(file).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(exists(file_before));
	BOOST_CHECK(!exists(file));
	BOOST_CHECK(exists(file_after));
}

/**
 * Delete a file with a unicode filename.
 */
BOOST_AUTO_TEST_CASE( delete_unicode_file )
{
	wpath unicode_file_name = Sandbox() / L"العربية.txt";
	ofstream(unicode_file_name).close();

	HRESULT hr = Provider()->Delete(
		Consumer().get(),
		bstr_t(ToRemotePath(unicode_file_name).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(!exists(unicode_file_name));
}

/**
 * Delete an empty directory.
 */
BOOST_AUTO_TEST_CASE( delete_empty_directory )
{
	wpath directory = Sandbox() / L"العربية";
	create_directory(directory);

	HRESULT hr = Provider()->DeleteDirectory(
		Consumer().get(), bstr_t(ToRemotePath(directory).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(!exists(directory));
}

/**
 * Delete a non-empty directory.  This is trickier as the contents have to be
 * deleted before the directory.
 */
BOOST_AUTO_TEST_CASE( delete_directory_recursively )
{
	wpath directory = Sandbox() / L"العربية";
	create_directory(directory);
	wpath file = directory / L"русский.txt";
	ofstream(file).close();

	HRESULT hr = Provider()->DeleteDirectory(
		Consumer().get(), bstr_t(ToRemotePath(directory).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(!exists(directory));
}

/**
 * Create a file with a unicode filename.
 */
BOOST_AUTO_TEST_CASE( create_file )
{
	wpath file = Sandbox() / L"漢字 العربية русский.txt";
	BOOST_CHECK(!exists(file));

	HRESULT hr = Provider()->CreateNewFile(
		Consumer().get(), bstr_t(ToRemotePath(file).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(exists(file));
}

/**
 * Create a directory with a unicode filename.
 */
BOOST_AUTO_TEST_CASE( create_directory )
{
	wpath file = Sandbox() / L"漢字 العربية русский 47";
	BOOST_CHECK(!exists(file));

	HRESULT hr = Provider()->CreateNewDirectory(
		Consumer().get(), bstr_t(ToRemotePath(file).string()).in());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(exists(file));
}

/**
 * Create a stream to a file with a unicode filename.
 *
 * Tests file creation as we don't create the file before the call and we
 * check that it exists after.
 */
BOOST_AUTO_TEST_CASE( get_file_stream )
{
	wpath file = Sandbox() / L"漢字 العربية русский.txt";
	BOOST_CHECK(!exists(file));

	com_ptr<IStream> stream;
	HRESULT hr = Provider()->GetFile(
		Consumer().get(), bstr_t(ToRemotePath(file).string()).in(),
		FALSE, stream.out());

	BOOST_REQUIRE_OK(hr);
	BOOST_CHECK(stream);
	BOOST_CHECK(exists(file));

	STATSTG statstg = STATSTG();
	hr = stream->Stat(&statstg, STATFLAG_DEFAULT);
	BOOST_CHECK_EQUAL(statstg.pwcsName, file.filename());
	::CoTaskMemFree(statstg.pwcsName);
	BOOST_REQUIRE_OK(hr);
}

BOOST_AUTO_TEST_SUITE_END()
