#!/usr/bin/env ruby

require "bakery/ports/bakery"

# boost is built from bakery port, bp-file is built from submodule source
topDir = File.dirname(File.expand_path(__FILE__));
$order = {
  :output_dir => File.join(topDir, "build"),
  :packages => ["boost", "bp-file"],
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
