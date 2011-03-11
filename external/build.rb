#!/usr/bin/env ruby

require "./bakery/ports/bakery"

topDir = File.dirname(File.expand_path(__FILE__));
$order = {
  :output_dir => File.join(topDir, "dist"),
  :packages => [
                "boost",
                "bp-file",
                "service_testing"
               ],
  :verbose => true,
  :use_source => {
    "bp-file"=>File.join(topDir, "bp-file")
  },
  :use_recipe => {
    "bp-file"=>File.join(topDir, "bp-file", "recipe.rb")
  }
}

b = Bakery.new $order
b.build
