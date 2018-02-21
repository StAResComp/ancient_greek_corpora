#!/usr/bin/perl -w

use strict;
use XML::LibXML;
use XML::LibXML::XPathContext;

# fields to read from STDIN
my @people = ();
my @text = ();
my @bib = ();
my @comments = ();

# in a multiline section?
my $where = "";

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
my $bodyXP = "/tei:TEI/tei:text/tei:body";

# load blank XML document
my $teiNS = "http://www.tei-c.org/ns/1.0";
my $blank_file = "blank_tei.xml";
my $dom = XML::LibXML->load_xml(location => $blank_file);

# set up XPath with TEI namespace
my $xp = XML::LibXML::XPathContext->new($dom);
$xp->registerNs("tei", $teiNS);

# reigns used in dates
my %reigns = (
    "Hadrian" => [117, 138],
    "Alexander Severus" => [222, 235],
    "Septimius Severus" => [192, 211],
    "Trajan" => [98, 117],
    "Caracalla" => [198, 217]
);

# read lines from STDIN
while (<STDIN>) {
		# remove line enedings
		chomp;
		
		# skip empty lines
		if (/^$/) {
				next;
		}
    # comments can contain things that look like field names
    elsif ($where eq "comments") {
        push @comments, $_;
    }
    else {
        # one liners
        if (/Source: *(.*)$/) {
            singleElement($1, $biblXP);
        }
        elsif (/^Date: *(.*)$/) {
            date($1, $dateXP);
        }
        elsif (/^Location: *(.*)$/) {
            pubPlace($1, $pubPlaceXP);
        }
        elsif (/^Title\/keyword: *(.*)$/) {
            singleElement($1, $titleXP);
        }
        elsif (/^Person of interest: *(.*)$/) {
            @people = split /, */, $1;
            arrayElement(\@people, $listPersonXP, "person");
        }
        # multiline headings
        elsif (/^Text/) {
            $where = "text";
        }
        elsif (/^Bibliography/) {
            $where = "bib";
        }
        elsif (/^Comments/) {
            $where = "comments";
        }
        # in a multiline section
        elsif ($where eq "text") {
            push @text, $_;
        }
        elsif ($where eq "bib") {
            push @bib, $_;
        }
		}
}

# put multiline values into DOM
arrayElement(\@comments, $notesStmtXP, "note");
arrayElement(\@bib, $listBiblXP, "bibl");
arrayElement(\@text, $bodyXP, "p");

# output DOM
print $dom->toString;

# split location into specific place and region
sub pubPlace { # =pod
    my $val = shift;
    my $elXP = shift;
    
    my $el = undef;
    my $settlement = "";
    my $region = "";
    my $settlementEl = undef;
    my $regionEl = undef;
    my $placeNameEl = undef;
    
    
    # remove any trailing spaces
    $val =~ s/\s*$//;
    
    if ($val) {
        ($el) = $xp->findnodes($elXP);
        
        # put full text in placeName
        $placeNameEl = $dom->createElementNS($teiNS, "placeName");
        $placeNameEl->appendTextNode($val);
        $el->appendChild($placeNameEl);
        
        # split on ,s
        my @places = split /,\s*/, $val;
        
        # settlement, region
        if (2 == @places) {
            $settlement = $places[0];
            $region = $places[1];
        }
        # settlement, region, region
        elsif (3 == @places) {
            $settlement = $places[0];
            $region = $places[2];
        }
        # region (if just one word)
        elsif (1 == @places && $places[0] !~ / /) {
            $region = $places[0];
        }
        
        # only want first word of region
        if ($region ne "") {
            $region =~ s/^(\S+)\s*.*/$1/;
        }
        
        if ($settlement ne "") {
            $settlementEl = $dom->createElementNS($teiNS, "settlement");
            $settlementEl->appendTextNode($settlement);
            $el->appendChild($settlementEl);
        }
        
        if ($region ne "") {
            $regionEl = $dom->createElementNS($teiNS, "region");
            $regionEl->appendTextNode($region);
            $el->appendChild($regionEl);
        }
    }
}
# =cut

# extract exact/ranges of dates from description
sub date { # =pod
    my $val = shift;
    my $elXP = shift;
    
    my $mult = 1; # default is AD
    my $start = undef; # @notBefore
    my $end = undef; # @notAfter
    my $exact = undef; # @when
    
    my $el = undef;
    
    if ($val) {
        ($el) = $xp->findnodes($elXP);
        $el->appendTextNode($val);
        
        # is it BC rather than AD
        if ($val =~ /\bBC\b|B\.C\./) {
            $mult = -1;
        }
        
        # date range
        if ($val =~ /([0-9]+)[-\/]([0-9]+)/) {
            $start = $1 * $mult;
            $end = $2 * $mult;
        }
        # range of centuries
        elsif ($val =~ /([1-4])(st|nd|rd|th)(-|\/| or )([1-4])(st|nd|rd|th) cent/) {
            if (-1 == $mult) {
                $start = ($1 * -100) + 1;
                $end =   ($4 * -100) + 100;
            }
            else {
                $start = ($1 * 100) - 100;
                $end =   ($4 * 100) - 1;
            }
            
            # modifier
            if ($val =~ /(early|mid|late|2nd half of the|1st half of the) ([1-4])(st|nd|rd|th)/i) {
                if ($1 =~ /early|1st half of the/i) {
                    $end -= 50 * $mult;
                }
                elsif ($1 =~ /mid/i) {
                    $start += 25 * $mult;
                    $end -= 25 * $mult;
                }
                elsif ($1 =~ /late|2nd half/i) {
                    $start += 50 * $mult;
                }
            }
        }
        # single century
        elsif ($val =~ /([1-4])(st|nd|rd|th) cent/) {
            $start = ($1 * 100 * $mult) - (100 * $mult);
            $end =   ($1 * 100 * $mult) - (1 * $mult);
            
            # modifier
            if ($val =~ /(early|mid|late|2nd half of the|1st half of the|middle of the|middle) ([1-4])(st|nd|rd|th)/i) {
                if ($1 =~ /early|1st half of the/i) {
                    $end -= 50 * $mult;
                }
                elsif ($1 =~ /mid|middle of the|middle/i) {
                    $start += 25 * $mult;
                    $end -= 25 * $mult;
                }
                elsif ($1 =~ /late|2nd half/i) {
                    $start += 50 * $mult;
                }
            }
        }
        # single year
        elsif ($val =~ /([0-9]+) (AD|BC|A\.D\.|B\.C\.)/) {
            $exact = $1 * $mult;
        }
        # reign of someone
        elsif ($val =~ /reign/ && $val =~ /(Hadrian|Alexander Severus|Septimius Severus|Trajan|Caracalla)/) {
            $start = $reigns{$1}[0];
            $end = $reigns{$1}[1];
        }
        
        # flip BC dates
        if (defined $start && defined $end && $start > $end) {
            my $t = $start;
            $start = $end;
            $end = $t;
        }
        
        # have attributes
        if (defined $start) {
            $el->setAttributeNode($dom->createAttribute("notBefore", $start));
        }
        if (defined $end) {
            $el->setAttributeNode($dom->createAttribute("notAfter", $end));
        }
        if (defined $exact) {
            $el->setAttributeNode($dom->createAttribute("when", $exact));
        }
    }
}
# =cut

# add text node to element indicated by xpath
sub singleElement { # =pod
		my $val = shift;
		my $elXP = shift;
		
		my $el = undef;
		
		if ($val) {
				($el) = $xp->findnodes($elXP);
				$el->appendTextNode($val);
		}
}
# =cut

# add elements using given name for each item in array at location given by xpath
sub arrayElement { # =pod
		my @vals = @{shift @_};
		my $parentXP = shift;
		my $elName = shift;
		
		my $parent = undef;
		my $child = undef;
		my $n = "";
    my @children = undef;

		if (@vals) {
				($parent) = $xp->findnodes($parentXP);
        
        # loop over text nodes
        foreach $n (@vals) {
            # body text requires more processing
            if ($parentXP eq $bodyXP) {
                @children = bodyText($elName, $n);
                foreach $child (@children) {
                    $parent->appendChild($child);
                }
            }
            # non-body text just gets added as text node
            else {
                $child = $dom->createElementNS($teiNS, $elName);
                $child->appendTextNode($n);
                $parent->appendChild($child);
            }
        }
		}
}
# =cut

# special handling of body text
sub bodyText { # =pod
    my $elName = shift;
    my $text = shift;
    
    # remove spaces between —s and multiple —s
    $text =~ s/-/\x{e2}\x{80}\x{94}/g;
    $text =~ s/\x{e2}\x{80}\x{94} /\x{e2}\x{80}\x{94}/g;
    $text =~ s/(\x{e2}\x{80}\x{94})+/\x{e2}\x{80}\x{94}/g;
    
    # convert NBS to spaces
    $text =~ s/\x{e2}\x{80}\x{83}/ /g;
    
    my $wEl = undef;
    my @els = ();
    my $first = 1;
    my $inAdd = 0;
    my ($w, $w1, $w2, $w3) = ("", "", "", "");
    my $haveW = 0;
    
    my $el = $dom->createElementNS($teiNS, $elName);
    
    # split text on spaces and zero-width gaps before some punctuation
    foreach $w (split /\s+|(?=[\.\,\(\)\?])|(?<=[\.\,\(\)\?])/, $text) {
        # first word is line number
        if ($first && $w =~ /^([0-9]+)$/) {
            $el->setAttributeNode($dom->createAttribute("n", $1));
            next;
        }
        
        # / and // indicate new lines, so start new parent element
        if ($w eq "/" || $w eq "//") {
            push @els, $el;
            $el = $dom->createElementNS($teiNS, $elName);
            next;
        }
        # word contains /
        # finish current line and start new one, removing /s from word
        elsif ($w =~ /\//) {
            push @els, $el;
            $el = $dom->createElementNS($teiNS, $elName);
            $w =~ s/\///g;
        }
        
        # not on first word anymore
        $first = 0;
        
        $wEl = $dom->createElementNS($teiNS, "w");
        $haveW = 0;

        # split w into before, [/] and after
        while (($w1, $w2, $w3) = $w =~ /^([^\[\]]*)([\[\]])(.*)$/) {
            # have text before [/]
            if ($w1 ne "") {
                $wEl = additions($w1, $wEl, $inAdd);
            }

            # opening [, so in addition
            $inAdd = ($w2 eq '[');
            
            # process rest of word after [/]
            $w = $w3;
            $haveW = 1;
        }
        
        # have something left
        if ($w ne "") {
            $haveW = 1;
            $wEl = additions($w, $wEl, $inAdd);
        }
      
        # add word to parent element
        if (1 == $haveW) {
            $el->appendChild($wEl);
        }
    }
    
    push @els, $el;
    
    return @els;
}
# =cut

sub additions { # =pod
    my ($t, $t1, $t2, $t3) = (shift, "", "", "");
    my $wEl = shift;
    my $inAdd = shift;
    
    my $addEl = undef, my $thisEl = undef, my $unclearEl = undef;

    # in an addition, so need an add element
    # and set thisEl to add element
    if ($inAdd) {
        $addEl = $dom->createElementNS($teiNS, "add");
        $thisEl = $addEl;
    }
    # thisEl is just w element
    else {
        $thisEl = $wEl;
    }
    
    # split text into before em-dash, em-dash and everything after
    while (($t1, $t2, $t3) = $t =~ /^(.?)(—)(.*)$/) {
        # text before —
        if ($t1 ne "") {
            $thisEl->appendTextNode($t1);
        }
        
        # have —
        if ($t2 eq "—") {
            $unclearEl = $dom->createElementNS($teiNS, "unclear");
            $unclearEl->appendTextNode("—");
            $thisEl->appendChild($unclearEl);
        }
        
        # continue with everything after —
        $t = $t3;
    }
    
    # have remaining text, so add to thisEl
    if ($t ne "") {
        $thisEl->appendTextNode($t);
    }
    
    # have add element to add to w element
    if (defined $addEl) {
        $wEl->appendChild($addEl);
    }
    
    return $wEl;
}
# =cut
