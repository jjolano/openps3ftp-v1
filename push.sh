#!/bin/bash

git add *
git rm push.sh
git add source/*
git add include/*

git commit -m 'Some changes'

git push
