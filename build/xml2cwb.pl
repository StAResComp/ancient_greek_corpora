#!/usr/bin/perl -w

use strict;
use DBI;
use XML::LibXML;
use XML::LibXML::XPathContext;
use Encode;
use File::Basename;

my $db = undef, my $stmt = undef;
my $dbFile = "", my $xmlFile = "";
my $outDom = undef;
my $teiNS = "http://www.tei-c.org/ns/1.0";
my $corpusEl = undef;

# XPath strings
my $fileDescXP = "/tei:TEI/tei:teiHeader/tei:fileDesc/";
my $titleXP = $fileDescXP . "tei:titleStmt/tei:title";
my $publicationStmtXP = $fileDescXP . "tei:publicationStmt/";
my $pubPlaceXP = $publicationStmtXP . "tei:pubPlace";
my $dateXP = $publicationStmtXP . "tei:date";
my $notesStmtXP = $fileDescXP . "tei:notesStmt";
my $sourceDescXP = $fileDescXP . "tei:sourceDesc";
my $biblXP = $sourceDescXP  . "/tei:bibl";
my $listBiblXP = $sourceDescXP . "/tei:listBibl";
my $listPersonXP = $sourceDescXP . "/tei:listPerson";
my $pXP = "/tei:TEI/tei:text/tei:body/tei:p";
my $vrtFile = "", my $csvFile = "";
my @metadataFields = ("id", "title", "from", "to", "settlement", "region", "person");

# get database file, VRT file and CSV file from ARGV
$dbFile = shift or die(usage($0));
$vrtFile = shift or die(usage($0));
$csvFile = shift or die(usage($0));

# open database
$db = DBI->connect("dbi:SQLite:dbname=$dbFile", "", "");
# prepare statement for finding words
$stmt = $db->prepare("SELECT lemma, postag FROM words WHERE form = ?");

# create output DOM
$outDom = XML::LibXML->createDocument;
$outDom->setEncoding("UTF-8");
$corpusEl = $outDom->createElement("corpus");
$corpusEl->appendTextNode("\n");
$outDom->setDocumentElement($corpusEl);

# open CSV file
open CSV, ">$csvFile" or die("Couldn't open $csvFile\n");

# loop over input documents
while ($xmlFile = shift) {
    print CSV metadata(inFile($xmlFile));
}

close CSV;

# output final DOM
$outDom->toFile($vrtFile, 0);

# return usage message
sub usage { # =pod
    my $cmd = shift;
    
    return "Usage: $cmd dbfile xmlfile\n";
}
# =cut

# takes hash reference and returns line for metadata CSV
sub metadata {
    my $metadata = shift;
    
    my $unknown = "unknown";
    my $field = "";
    my $line = "";
    my $fmt = "%s\t%s\t%s\t%s\t%s\t%s\t%s\n";
    
    foreach $field (@metadataFields) {
        if (not defined $metadata->{$field}) {
            $metadata->{$field} = $unknown;
        }
    }
    
    return sprintf $fmt, $metadata->{"id"}, $metadata->{"title"}, 
      $metadata->{"from"}, $metadata->{"to"}, $metadata->{"settlement"},
      $metadata->{"region"}, $metadata->{"person"};
}

# process input XML document
# and return data for metadata CSV
sub inFile { # =pod
    my $xmlFile = shift;
    
    my $dom = undef, my $xp = undef;
    my $textEl = undef, my $metaEl = undef;
    my @pEls = (), my $pEl = undef;
    my $sText = "", my $sEl = undef;
    my @wEls = undef, my $wEl = undef, my $wText = "";
    my $path = "", my $filename = "", my $suffix = "";
    my %metadata = (), my $value = "";
    
    # split xmlFile into filename, path and suffix
    ($filename, $path, $suffix) = fileparse($xmlFile, ".xml");
    
    # open input XML document
    $dom = XML::LibXML->load_xml(location => $xmlFile);
    $xp = XML::LibXML::XPathContext->new($dom);
    $xp->registerNs("tei", $teiNS);

    # create text element
    $textEl = $outDom->createElement("text");

    # sanitise filename
    $filename =~ s/[^a-zA-Z0-9_]/_/g;
    $textEl->setAttributeNode($outDom->createAttribute("id", $filename));
    $metadata{"id"} = $filename;
    
    # add title
    ($metaEl) = $xp->findnodes($titleXP);
    $textEl->setAttribute("title", $metaEl->textContent);
    $metadata{"title"} = $metaEl->textContent;
    
    # add dates
    ($metaEl) = $xp->findnodes($dateXP);
    
    if ($metaEl->hasAttribute("when")) {
        $textEl->setAttribute("from", $metaEl->getAttribute("when"));
        $textEl->setAttribute("to", $metaEl->getAttribute("when"));
        $metadata{"from"} = $metaEl->getAttribute("when");
        $metadata{"to"} = $metaEl->getAttribute("when");
    }
    elsif ($metaEl->hasAttribute("notBefore") && $metaEl->hasAttribute("notAfter")) {
        $textEl->setAttribute("from", $metaEl->getAttribute("notBefore"));
        $textEl->setAttribute("to", $metaEl->getAttribute("notAfter"));
        $metadata{"from"} = $metaEl->getAttribute("notBefore");
        $metadata{"to"} = $metaEl->getAttribute("notAfter");
    }
    
    # add settlement
    ($metaEl) = $xp->findnodes($pubPlaceXP . "/tei:settlement");
    if ($metaEl) {
        $value = $metaEl->textContent;
        $value =~ s/[^a-zA-Z0-9_]/_/g;
        $textEl->setAttribute("settlement", $value);
        $metadata{"settlement"} = $value;
    }
    
    # add region
    ($metaEl) = $xp->findnodes($pubPlaceXP . "/tei:region");
    if ($metaEl) {
        $value = $metaEl->textContent;
        $value =~ s/[^a-zA-Z0-9_]/_/g;
        $textEl->setAttribute("region", $value);
        $metadata{"region"} = $value;
    }
    
    # add person of interest
    ($metaEl) = $xp->findnodes($listPersonXP . "/tei:person[1]");
    if ($metaEl) {
        $textEl->setAttribute("person", $metaEl->textContent);
        $metadata{"person"} = $metaEl->textContent;
    }
    
    # loop over paragraphs
    @pEls = $xp->findnodes($pXP);
    foreach $pEl (@pEls) {
        $sEl = $outDom->createElement("s");
        $sText = "\n";
        
        # loop over words in paragraph
        @wEls = $pEl->childNodes();
        foreach $wEl (@wEls) {
            $wText = $wEl->textContent;
            if ($wText ne "") {
                $sText .= word($wText);
            }
        }
     
        # add text to s element and add s element to text
        $sEl->appendTextNode($sText);
        $textEl->appendTextNode("\n");
        $textEl->appendChild($sEl);
    }

    # add text element to corpus element
    $textEl->appendTextNode("\n");
    $corpusEl->appendChild($textEl);
    $corpusEl->appendTextNode("\n");
    
    return \%metadata;
}
# =cut

# look up word in database and output line giving word, POS and lemma
sub word { # =pod
    my $word = shift;
    
    $stmt->bind_param(1, $word);
    $stmt->execute();
    my ($lemma, $postag) = $stmt->fetchrow_array;
    
    if (not defined $postag) {
        $postag = "-";
    }
    if (not defined $lemma) {
        $lemma = $word;
    }
    else {
        $lemma = decode_utf8($lemma);
    }
    
    return sprintf "%s\t%s\t%s\n", $word, $postag, $lemma;
}
# =cut
