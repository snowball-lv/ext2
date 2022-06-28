#!/usr/bin/env ruby

blocks = ARGV[0].to_i
blocks.times do |i|
    str = "[block #{i}]"
    rem = 1024 - str.length - 2
    puts str + "\0" * rem + "-"
end
